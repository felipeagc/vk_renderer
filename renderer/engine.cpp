#include "engine.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <rg.h>
#include "allocator.h"
#include "platform.h"
#include "lexer.h"
#include "string_map.hpp"
#include "config.h"
#include "pipeline_util.h"
#include "pbr.h"
#include "pool.h"
#include <tinyshader/tinyshader.h>

#if defined(_MSC_VER)
#pragma warning(disable:4996)
#endif

#ifdef __linux__
#include <unistd.h>
#include <linux/limits.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static const char *getExeDirPath(Allocator *allocator)
{
#if defined(__linux__)
    char buf[PATH_MAX];
    size_t buf_size = readlink("/proc/self/exe", buf, sizeof(buf));

    char *path = (char*)Allocate(allocator, buf_size+1);
    memcpy(path, buf, buf_size);
    path[buf_size] = '\0';

    size_t last_slash_pos = 0;
    for (size_t i = 0; i < buf_size; ++i)
    {
        if (path[i] == '/')
        {
            last_slash_pos = i;
        }
    }

    path[last_slash_pos] = '\0';

    return path;
#elif defined(_WIN32)
    char tmp_buf[MAX_PATH];
    DWORD buf_size = GetModuleFileNameA(NULL, tmp_buf, sizeof(tmp_buf));
    char *path = (char*)Allocate(allocator, buf_size+1);
    GetModuleFileNameA(GetModuleHandle(NULL), path, buf_size+1);

    size_t last_slash_pos = 0;
    for (size_t i = 0; i < buf_size; ++i)
    {
        if (path[i] == '\\')
        {
            last_slash_pos = i;
        }
    }

    path[last_slash_pos] = '\0';

    return path;
#else
    #error unsupported system
#endif
}

struct Engine
{
    Allocator *allocator;
    Arena *arena;
    Platform *platform;

    const char *exe_dir;

    RgCmdPool *graphics_cmd_pool;
    RgCmdPool *transfer_cmd_pool;
    ImageHandle white_image;
    ImageHandle black_image;
    SamplerHandle default_sampler;

    ImageHandle brdf_image;

    RgDescriptorSetLayout *global_set_layout;
    RgPipelineLayout *global_pipeline_layout;
    RgDescriptorSet *global_descriptor_set;

    Pool *storage_buffer_pool;
    Pool *texture_pool;
    Pool *sampler_pool;
};

extern "C" Engine *EngineCreate(Allocator *allocator)
{
    Engine *engine = (Engine*)Allocate(allocator, sizeof(Engine));
    *engine = {};

    engine->allocator = allocator;
    engine->platform = PlatformCreate(allocator, "App");
    engine->arena = ArenaCreate(engine->allocator, 4194304); // 4MiB

    engine->exe_dir = getExeDirPath(allocator);

    RgDevice *device = PlatformGetDevice(engine->platform);

    engine->storage_buffer_pool = PoolCreate(engine->allocator, 4 * 1024);
    engine->texture_pool = PoolCreate(engine->allocator, 4 * 1024);
    engine->sampler_pool = PoolCreate(engine->allocator, 4 * 1024);

    {
        RgDescriptorSetLayoutEntry entries[] = {
            {
                0, // binding
                RG_DESCRIPTOR_STORAGE_BUFFER, // type
                RG_SHADER_STAGE_ALL, // shader_stages
                PoolGetSlotCount(engine->storage_buffer_pool), // count
            },
            {
                1, // binding
                RG_DESCRIPTOR_IMAGE, // type
                RG_SHADER_STAGE_ALL, // shader_stages
                PoolGetSlotCount(engine->texture_pool), // count
            },
            {
                2, // binding
                RG_DESCRIPTOR_SAMPLER, // type
                RG_SHADER_STAGE_ALL, // shader_stages
                PoolGetSlotCount(engine->sampler_pool), // count
            },
        };

        RgDescriptorSetLayoutInfo info = {};
        info.entries = entries;
        info.entry_count = sizeof(entries) / sizeof(entries[0]);

        engine->global_set_layout = rgDescriptorSetLayoutCreate(device, &info);

        RgPipelineLayoutInfo pipeline_layout_info = {};
        pipeline_layout_info.set_layouts = &engine->global_set_layout;
        pipeline_layout_info.set_layout_count = 1;
        engine->global_pipeline_layout = rgPipelineLayoutCreate(device, &pipeline_layout_info);

        engine->global_descriptor_set = rgDescriptorSetCreate(device, engine->global_set_layout);
    }

    engine->transfer_cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_TRANSFER);
    engine->graphics_cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);

    RgImageInfo image_info = {};
    image_info.extent = {1, 1, 1};
    image_info.format = RG_FORMAT_RGBA8_UNORM;
    image_info.usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_TRANSFER_DST;
    image_info.aspect = RG_IMAGE_ASPECT_COLOR;
    image_info.sample_count = 1;
    image_info.mip_count = 1;
    image_info.layer_count = 1;

    engine->white_image = EngineAllocateImageHandle(engine, &image_info);
    engine->black_image = EngineAllocateImageHandle(engine, &image_info);

    uint8_t white_data[] = {255, 255, 255, 255};
    uint8_t black_data[] = {0, 0, 0, 255};

    RgImageCopy image_copy = {};
    RgExtent3D extent = {1, 1, 1};

    image_copy.image = engine->white_image.image;
    rgImageUpload(
            device,
            engine->transfer_cmd_pool,
            &image_copy,
            &extent,
            sizeof(white_data),
            white_data);

    image_copy.image = engine->black_image.image;
    rgImageUpload(
            device,
            engine->transfer_cmd_pool,
            &image_copy,
            &extent,
            sizeof(black_data),
            black_data);

    RgSamplerInfo sampler_info = {};
    sampler_info.anisotropy = true;
    sampler_info.max_anisotropy = 16.0f;
    sampler_info.min_filter = RG_FILTER_LINEAR;
    sampler_info.mag_filter = RG_FILTER_LINEAR;
    sampler_info.address_mode = RG_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.border_color = RG_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    engine->default_sampler = EngineAllocateSamplerHandle(engine, &sampler_info);

    engine->brdf_image = GenerateBRDFLUT(engine, engine->graphics_cmd_pool, 512);

    return engine;
}

extern "C" void EngineDestroy(Engine *engine)
{
    RgDevice *device = PlatformGetDevice(engine->platform);

    rgPipelineLayoutDestroy(device, engine->global_pipeline_layout);
    rgDescriptorSetDestroy(device, engine->global_descriptor_set);
    rgDescriptorSetLayoutDestroy(device, engine->global_set_layout);

	EngineFreeImageHandle(engine, &engine->brdf_image);
	EngineFreeImageHandle(engine, &engine->white_image);
	EngineFreeImageHandle(engine, &engine->black_image);
	EngineFreeSamplerHandle(engine, &engine->default_sampler);
    rgCmdPoolDestroy(device, engine->transfer_cmd_pool);
    rgCmdPoolDestroy(device, engine->graphics_cmd_pool);

    PoolDestroy(engine->storage_buffer_pool);
    PoolDestroy(engine->texture_pool);
    PoolDestroy(engine->sampler_pool);

    PlatformDestroy(engine->platform);

    ArenaDestroy(engine->arena);

    Free(engine->allocator, (void*)engine->exe_dir);
    Free(engine->allocator, engine);
}

extern "C" Platform *EngineGetPlatform(Engine *engine)
{
    return engine->platform;
}

extern "C" const char *EngineGetExeDir(Engine *engine)
{
    return engine->exe_dir;
}

extern "C" uint8_t *EngineLoadFileRelative(
    Engine *engine, Allocator *allocator, const char *relative_path, size_t *size)
{
    size_t path_size = strlen(engine->exe_dir) + 1 + strlen(relative_path) + 1;
    char *path = (char*)Allocate(allocator, path_size);
    snprintf(path, path_size, "%s/%s", engine->exe_dir, relative_path);
    path[path_size-1] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = (uint8_t*)Allocate(allocator, *size);
    assert(fread(data, 1, *size, f) == *size);

    fclose(f);

    Free(allocator, path);

    return data;
}

extern "C" ImageHandle EngineGetWhiteImage(Engine *engine)
{
    return engine->white_image;
}

extern "C" ImageHandle EngineGetBlackImage(Engine *engine)
{
    return engine->black_image;
}

extern "C" ImageHandle EngineGetBRDFImage(Engine *engine)
{
    return engine->brdf_image;
}

extern "C" SamplerHandle EngineGetDefaultSampler(Engine *engine)
{
    return engine->default_sampler;
}

extern "C" RgPipeline *EngineCreateGraphicsPipeline(Engine *engine, const char *path)
{
    RgPipelineLayout *pipeline_layout = engine->global_pipeline_layout;
    assert(pipeline_layout);

    size_t hlsl_size = 0;
    char *hlsl = (char*)
        EngineLoadFileRelative(engine, engine->allocator, path, &hlsl_size);
    assert(hlsl);

    RgPipeline *pipeline =
        PipelineUtilCreateGraphicsPipeline(engine, engine->allocator, pipeline_layout, hlsl, hlsl_size);
    assert(pipeline);

    Free(engine->allocator, hlsl);

    return pipeline;
}

extern "C" RgPipeline *EngineCreateComputePipeline(Engine *engine, const char *path)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

    RgPipelineLayout *pipeline_layout = engine->global_pipeline_layout;
    assert(pipeline_layout);

    size_t hlsl_size = 0;
    char *hlsl = (char*) EngineLoadFileRelative(engine, engine->allocator, path, &hlsl_size);
    assert(hlsl);

    uint8_t *spv_code = NULL;
    size_t spv_code_size = 0;

    {
        TsCompilerOptions *options = tsCompilerOptionsCreate();
        tsCompilerOptionsSetStage(options, TS_SHADER_STAGE_VERTEX);
        const char *entry_point = "main";
        tsCompilerOptionsSetEntryPoint(options, entry_point, strlen(entry_point));
        tsCompilerOptionsSetSource(options, hlsl, hlsl_size, NULL, 0);

        TsCompilerOutput *output = tsCompile(options);
        const char *errors = tsCompilerOutputGetErrors(output);
        if (errors)
        {
            fprintf(stderr, "Shader compilation error:\n%s\n", errors);

            tsCompilerOutputDestroy(output);
            tsCompilerOptionsDestroy(options);
            exit(1);
        }

        size_t spirv_size = 0;
        const uint8_t *spirv = tsCompilerOutputGetSpirv(output, &spirv_size);

        spv_code = (uint8_t*)Allocate(engine->allocator, spirv_size);
        memcpy(spv_code, spirv, spirv_size);
        spv_code_size = spirv_size;

        tsCompilerOutputDestroy(output);
        tsCompilerOptionsDestroy(options);
    }

    RgComputePipelineInfo info = {};
    info.pipeline_layout = pipeline_layout;
    info.code = spv_code;
    info.code_size = spv_code_size;
    RgPipeline *pipeline = rgComputePipelineCreate(device, &info);
    assert(pipeline);

    Free(engine->allocator, spv_code);
    Free(engine->allocator, hlsl);

    return pipeline;
}

RgPipelineLayout *EngineGetGlobalPipelineLayout(Engine *engine)
{
    return engine->global_pipeline_layout;
}

RgDescriptorSet *EngineGetGlobalDescriptorSet(Engine *engine)
{
    return engine->global_descriptor_set;
}

static uint32_t EngineAllocateDescriptor(
    Engine *engine,
    Pool *pool,
    uint32_t binding,
    const RgDescriptor *descriptor)
{
    assert(engine->global_descriptor_set);

    uint32_t handle = PoolAllocateSlot(pool);
    if (handle == UINT32_MAX) return handle;

    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

    RgDescriptorUpdateInfo entry = {};
    entry.binding = binding;
    entry.base_index = handle;
    entry.descriptor_count = 1;
    entry.descriptors = descriptor;

    rgDescriptorSetUpdate(device, engine->global_descriptor_set, &entry, 1);

    return handle;
}

static void EngineFreeDescriptor(Engine *engine, Pool *pool, uint32_t handle)
{
    (void)engine;
    PoolFreeSlot(pool, handle);
}

extern "C" BufferHandle EngineAllocateStorageBufferHandle(Engine *engine, RgBufferInfo *info)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

	BufferHandle handle = {};
	handle.buffer = rgBufferCreate(device, info);

    RgDescriptor descriptor = {};
    descriptor.buffer.buffer = handle.buffer;
    descriptor.buffer.offset = 0;
    descriptor.buffer.size = 0;

	handle.index = EngineAllocateDescriptor(
        engine,
        engine->storage_buffer_pool,
        0,
        &descriptor);

	assert(handle.index != UINT32_MAX);

	return handle;
}

extern "C" void EngineFreeStorageBufferHandle(Engine *engine, BufferHandle *handle)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);
    EngineFreeDescriptor(engine, engine->storage_buffer_pool, handle->index);
	rgBufferDestroy(device, handle->buffer);
}

extern "C" ImageHandle EngineAllocateImageHandle(Engine *engine, RgImageInfo *info)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

	ImageHandle handle = {};
	handle.image = rgImageCreate(device, info);

    RgDescriptor descriptor = {};
    descriptor.image.image = handle.image;

	handle.index = EngineAllocateDescriptor(
        engine,
        engine->texture_pool,
        1,
        &descriptor);

	assert(handle.index != UINT32_MAX);

	return handle;
}

extern "C" void EngineFreeImageHandle(Engine *engine, ImageHandle *handle)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);
    EngineFreeDescriptor(engine, engine->texture_pool, handle->index);
	rgImageDestroy(device, handle->image);
}

extern "C" SamplerHandle EngineAllocateSamplerHandle(Engine *engine, RgSamplerInfo *info)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

	SamplerHandle handle = {};
	handle.sampler = rgSamplerCreate(device, info);

    RgDescriptor descriptor = {};
    descriptor.image.sampler = handle.sampler;

	handle.index = EngineAllocateDescriptor(
        engine,
        engine->sampler_pool,
        2,
        &descriptor);

	assert(handle.index != UINT32_MAX);

	return handle;
}

extern "C" void EngineFreeSamplerHandle(Engine *engine, SamplerHandle *handle)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);
    EngineFreeDescriptor(engine, engine->sampler_pool, handle->index);
	rgSamplerDestroy(device, handle->sampler);
}

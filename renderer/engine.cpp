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
    RgImage *white_image;
    RgImage *black_image;
    RgSampler *default_sampler;

    RgImage *brdf_image;

    StringMap<RgPipelineLayout *> pipeline_layout_map;
    StringMap<RgDescriptorSetLayout *> set_layout_map;

    RgDescriptorSetLayout *global_set_layout;
    RgPipelineLayout *global_pipeline_layout;
    RgDescriptorSet *global_descriptor_set;

    Pool *storage_buffer_pool;
    Pool *texture_pool;
    Pool *sampler_pool;
};

static void LoadConfig(Engine *engine, const char *spec, size_t spec_size)
{
    Allocator *allocator = engine->allocator;
    Allocator *arena = ArenaGetAllocator(engine->arena);
    RgDevice *device = PlatformGetDevice(engine->platform);

    Config *config = ConfigParse(allocator, spec, spec_size);
    if (!config) exit(1);

    // Print config
    {
        const char *config_str = ConfigSprint(config, allocator);
        printf("%s\n", config_str);
        Free(allocator, (void*)config_str);
    }

    ConfigValue *root = ConfigGetRoot(config);
    ConfigValue *set_layouts_field = ConfigValueObjectGetField(root, "set_layouts");
    assert(set_layouts_field);
    assert(ConfigValueGetType(set_layouts_field) == CONFIG_VALUE_OBJECT);

    ConfigValue *pipeline_layouts_field = ConfigValueObjectGetField(root, "pipeline_layouts");
    assert(pipeline_layouts_field);
    assert(ConfigValueGetType(pipeline_layouts_field) == CONFIG_VALUE_OBJECT);

    {
        ConfigValue **values = nullptr;
        const char **names = nullptr;

        size_t length = ConfigValueObjectGetAllFields(
            set_layouts_field,
            allocator,
            &names,
            &values);

        for (size_t i = 0; i < length; ++i)
        {
            const char *name = names[i];
            ConfigValue *value = values[i];

            assert(ConfigValueGetType(value) == CONFIG_VALUE_ARRAY);

            size_t binding_count = ConfigValueArrayGetLength(value);

            RgDescriptorSetLayoutEntry *entries =
                (RgDescriptorSetLayoutEntry *)
                Allocate(allocator, sizeof(RgDescriptorSetLayoutEntry) * binding_count);

            for (size_t j = 0; j < binding_count; ++j)
            {
                ConfigValue *binding_value = ConfigValueArrayGetElement(value, j);
                assert(binding_value);
                assert(ConfigValueGetType(binding_value) == CONFIG_VALUE_OBJECT);

                entries[j] = {};
                entries[j].binding = j;
                entries[j].count = 1;

                ConfigValue *type_value = ConfigValueObjectGetField(binding_value, "type");
                assert(type_value);
                assert(ConfigValueGetType(type_value) == CONFIG_VALUE_STRING);
                ConfigValue *stages_value = ConfigValueObjectGetField(binding_value, "stages");
                assert(stages_value);
                assert(ConfigValueGetType(stages_value) == CONFIG_VALUE_ARRAY);

                const char *binding_type_string = ConfigValueGetString(type_value);
                if (strcmp(binding_type_string, "uniform_buffer") == 0)
                {
                    entries[j].type = RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC;
                }
                else if (strcmp(binding_type_string, "image") == 0)
                {
                    entries[j].type = RG_DESCRIPTOR_IMAGE;
                }
                else if (strcmp(binding_type_string, "sampler") == 0)
                {
                    entries[j].type = RG_DESCRIPTOR_SAMPLER;
                }

                size_t stage_count = ConfigValueArrayGetLength(stages_value);
                for (size_t k = 0; k < stage_count; ++k)
                {
                    ConfigValue *stage_value = ConfigValueArrayGetElement(stages_value, k);
                    assert(stage_value);
                    assert(ConfigValueGetType(stage_value) == CONFIG_VALUE_STRING);

                    const char *stage_string = ConfigValueGetString(stage_value);
                    if (strcmp(stage_string, "vertex") == 0)
                    {
                        entries[j].shader_stages |= RG_SHADER_STAGE_VERTEX;
                    }
                    else if (strcmp(stage_string, "fragment") == 0)
                    {
                        entries[j].shader_stages |= RG_SHADER_STAGE_FRAGMENT;
                    }
                    else if (strcmp(stage_string, "compute") == 0)
                    {
                        entries[j].shader_stages |= RG_SHADER_STAGE_COMPUTE;
                    }
                }
            }

            RgDescriptorSetLayoutInfo info = {};
            info.entries = entries;
            info.entry_count = binding_count;
            auto set_layout = rgDescriptorSetLayoutCreate(device, &info);

            engine->set_layout_map.set(Strdup(arena, name), set_layout);
            Free(allocator, entries);
        }

        Free(allocator, (void*)values);
        Free(allocator, (void*)names);
    }

    {
        ConfigValue **values = nullptr;
        const char **names = nullptr;

        size_t length = ConfigValueObjectGetAllFields(
            pipeline_layouts_field,
            allocator,
            &names,
            &values);

        for (size_t i = 0; i < length; ++i)
        {
            const char *name = names[i];
            ConfigValue *value = values[i];

            assert(ConfigValueGetType(value) == CONFIG_VALUE_OBJECT);

            ConfigValue *set_layouts_field = ConfigValueObjectGetField(value, "set_layouts");
            assert(set_layouts_field);
            assert(ConfigValueGetType(set_layouts_field) == CONFIG_VALUE_ARRAY);
            size_t set_layout_count = ConfigValueArrayGetLength(set_layouts_field);

            RgDescriptorSetLayout **set_layouts = (RgDescriptorSetLayout **)
                Allocate(allocator, sizeof(RgDescriptorSetLayout *) * set_layout_count);

            for (size_t j = 0; j < set_layout_count; ++j)
            {
                ConfigValue *set_layout_name_value = ConfigValueArrayGetElement(set_layouts_field, j);
                assert(set_layout_name_value);
                assert(ConfigValueGetType(set_layout_name_value) == CONFIG_VALUE_STRING);
                const char *set_layout_name = ConfigValueGetString(set_layout_name_value);

                engine->set_layout_map.get(set_layout_name, &set_layouts[j]);
            }

            RgPipelineLayoutInfo info = {};
            info.set_layouts = set_layouts;
            info.set_layout_count = set_layout_count;
            RgPipelineLayout *pipeline_layout = rgPipelineLayoutCreate(device, &info);

            engine->pipeline_layout_map.set(Strdup(arena, name), pipeline_layout);

            Free(allocator, set_layouts);
        }
        
        Free(allocator, (void*)values);
        Free(allocator, (void*)names);
    }

    ConfigFree(config);
}

extern "C" Engine *EngineCreate(Allocator *allocator, const char *spec, size_t spec_size)
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

    engine->pipeline_layout_map = StringMap<RgPipelineLayout *>::create(allocator);
    engine->set_layout_map = StringMap<RgDescriptorSetLayout *>::create(allocator);

    LoadConfig(engine, spec, spec_size);

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

    engine->white_image = rgImageCreate(device, &image_info);
    engine->black_image = rgImageCreate(device, &image_info);

    uint8_t white_data[] = {255, 255, 255, 255};
    uint8_t black_data[] = {0, 0, 0, 255};

    RgImageCopy image_copy = {};
    RgExtent3D extent = {1, 1, 1};

    image_copy.image = engine->white_image;
    rgImageUpload(
            device,
            engine->transfer_cmd_pool,
            &image_copy,
            &extent,
            sizeof(white_data),
            white_data);

    image_copy.image = engine->black_image;
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
    engine->default_sampler = rgSamplerCreate(device, &sampler_info);

    engine->brdf_image = GenerateBRDFLUT(engine, engine->graphics_cmd_pool, 512);

    return engine;
}

extern "C" void EngineDestroy(Engine *engine)
{
    RgDevice *device = PlatformGetDevice(engine->platform);

    for (auto &slot : engine->pipeline_layout_map)
    {
        rgPipelineLayoutDestroy(device, slot.value);
    }

    for (auto &slot : engine->set_layout_map)
    {
        rgDescriptorSetLayoutDestroy(device, slot.value);
    }

    rgPipelineLayoutDestroy(device, engine->global_pipeline_layout);
    rgDescriptorSetDestroy(device, engine->global_descriptor_set);
    rgDescriptorSetLayoutDestroy(device, engine->global_set_layout);

    rgImageDestroy(device, engine->brdf_image);
    rgImageDestroy(device, engine->white_image);
    rgImageDestroy(device, engine->black_image);
    rgSamplerDestroy(device, engine->default_sampler);
    rgCmdPoolDestroy(device, engine->transfer_cmd_pool);
    rgCmdPoolDestroy(device, engine->graphics_cmd_pool);

    engine->pipeline_layout_map.free();
    engine->set_layout_map.free();

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

extern "C" RgDescriptorSetLayout *EngineGetSetLayout(Engine *engine, const char *name)
{
    RgDescriptorSetLayout *set_layout = nullptr;
    engine->set_layout_map.get(name, &set_layout);
    assert(set_layout);
    return set_layout;
}

extern "C" RgPipelineLayout *EngineGetPipelineLayout(Engine *engine, const char *name)
{
    RgPipelineLayout *pipeline_layout = nullptr;
    engine->pipeline_layout_map.get(name, &pipeline_layout);
    assert(pipeline_layout);
    return pipeline_layout;
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

extern "C" RgImage *EngineGetWhiteImage(Engine *engine)
{
    return engine->white_image;
}

extern "C" RgImage *EngineGetBlackImage(Engine *engine)
{
    return engine->black_image;
}

extern "C" RgImage *EngineGetBRDFImage(Engine *engine)
{
    return engine->brdf_image;
}

extern "C" RgSampler *EngineGetDefaultSampler(Engine *engine)
{
    return engine->default_sampler;
}

extern "C" RgPipeline *EngineCreateGraphicsPipeline(Engine *engine, const char *path, const char *type)
{
    RgPipelineLayout *pipeline_layout = EngineGetPipelineLayout(engine, type);
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

extern "C" RgPipeline *EngineCreateComputePipeline(Engine *engine, const char *path, const char *type)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

    RgPipelineLayout *pipeline_layout = EngineGetPipelineLayout(engine, type);
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

extern "C" RgPipeline *EngineCreateGraphicsPipeline2(Engine *engine, const char *path)
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

#include "engine.h"

#include <string.h>
#include <stdio.h>
#include <rg.h>
#include "allocator.h"
#include "platform.h"

#ifdef __linux__
#include <unistd.h>
#include <linux/limits.h>
#endif

static const char *getExeDirPath(Allocator *allocator)
{
#ifdef __linux__
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
#endif
}

struct Engine
{
    Allocator *allocator;
    Platform *platform;

    const char *exe_dir;

    RgCmdPool *transfer_cmd_pool;
    RgImage *white_image;
    RgImage *black_image;
    RgSampler *default_sampler;

    RgDescriptorSetLayout *bind_group_layouts[BIND_GROUP_MAX];
};

Engine *EngineCreate(Allocator *allocator)
{
    Engine *engine = (Engine*)Allocate(allocator, sizeof(Engine));
    *engine = {};

    engine->allocator = allocator;
    engine->platform = PlatformCreate(allocator, "App");

    engine->exe_dir = getExeDirPath(allocator);

    RgDevice *device = PlatformGetDevice(engine->platform);

    {
        RgDescriptorSetLayoutEntry entries[] = {
            {
                .binding = 0,
                .type = RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
        };

        RgDescriptorSetLayoutInfo info = {};

        info.entries = entries;
        info.entry_count = sizeof(entries)/sizeof(entries[0]);

        engine->bind_group_layouts[BIND_GROUP_CAMERA] =
            rgDescriptorSetLayoutCreate(device, &info);
    }

    {
        RgDescriptorSetLayoutEntry entries[] = {
            {
                .binding = 0,
                .type = RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 1,
                .type = RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 2,
                .type = RG_DESCRIPTOR_SAMPLER,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 3,
                .type = RG_DESCRIPTOR_IMAGE,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 4,
                .type = RG_DESCRIPTOR_IMAGE,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 5,
                .type = RG_DESCRIPTOR_IMAGE,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 6,
                .type = RG_DESCRIPTOR_IMAGE,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
            {
                .binding = 7,
                .type = RG_DESCRIPTOR_IMAGE,
                .shader_stages = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
                .count = 1,
            },
        };

        RgDescriptorSetLayoutInfo info = {};
        info.entries = entries;
        info.entry_count = sizeof(entries) / sizeof(entries[0]);

        engine->bind_group_layouts[BIND_GROUP_MODEL] =
            rgDescriptorSetLayoutCreate(device, &info);
    }

    engine->transfer_cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_TRANSFER);

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

    uint8_t white_data[] = {0, 0, 0, 255};
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

    return engine;
}

void EngineDestroy(Engine *engine)
{
    RgDevice *device = PlatformGetDevice(engine->platform);

    for (uint32_t i = 0; i < BIND_GROUP_MAX; ++i)
    {
        rgDescriptorSetLayoutDestroy(device, engine->bind_group_layouts[i]);
    }

    rgImageDestroy(device, engine->white_image);
    rgImageDestroy(device, engine->black_image);
    rgSamplerDestroy(device, engine->default_sampler);
    rgCmdPoolDestroy(device, engine->transfer_cmd_pool);

    PlatformDestroy(engine->platform);

    Free(engine->allocator, (void*)engine->exe_dir);
    Free(engine->allocator, engine);
}

Platform *EngineGetPlatform(Engine *engine)
{
    return engine->platform;
}

RgDescriptorSetLayout *EngineGetSetLayout(Engine *engine, BindGroupType type)
{
    return engine->bind_group_layouts[type];
}

const char *EngineGetExeDir(Engine *engine)
{
    return engine->exe_dir;
}

uint8_t *EngineLoadFileRelative(Engine *engine, const char *relative_path, size_t *size)
{
    size_t path_size = strlen(engine->exe_dir) + 1 + strlen(relative_path) + 1;
    char *path = new char[path_size];
    snprintf(path, path_size, "%s/%s", engine->exe_dir, relative_path);
    path[path_size-1] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = new uint8_t[*size];
    fread(data, *size, 1, f);

    fclose(f);

    delete[] path;

    return data;
}

RgImage *EngineGetWhiteImage(Engine *engine)
{
    return engine->white_image;
}

RgImage *EngineGetBlackImage(Engine *engine)
{
    return engine->black_image;
}

RgSampler *EngineGetDefaultSampler(Engine *engine)
{
    return engine->default_sampler;
}


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

    RgCmdPool *transfer_cmd_pool;
    RgImage *white_image;
    RgImage *black_image;
    RgSampler *default_sampler;

    StringMap<RgPipelineLayout *> pipeline_layout_map;
    StringMap<RgDescriptorSetLayout *> set_layout_map;
};

static void LoadConfig(Engine *engine, const char *config_path)
{
    Allocator *allocator = engine->allocator;
    Allocator *arena = ArenaGetAllocator(engine->arena);
    RgDevice *device = PlatformGetDevice(engine->platform);
    
    size_t text_size = 0;
    const char *text = (const char *)
        EngineLoadFileRelative(engine, allocator, config_path, &text_size);

    Config *config = ConfigParse(allocator, text, text_size);
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
    Free(allocator, (void*)text);
}

Engine *EngineCreate(Allocator *allocator)
{
    Engine *engine = (Engine*)Allocate(allocator, sizeof(Engine));
    *engine = {};

    engine->allocator = allocator;
    engine->platform = PlatformCreate(allocator, "App");
    engine->arena = ArenaCreate(engine->allocator, 4194304); // 4MiB

    engine->exe_dir = getExeDirPath(allocator);

    RgDevice *device = PlatformGetDevice(engine->platform);

    engine->pipeline_layout_map = StringMap<RgPipelineLayout *>::create(allocator);
    engine->set_layout_map = StringMap<RgDescriptorSetLayout *>::create(allocator);

    LoadConfig(engine, "../spec.json");

    //
    // Create pipeline layouts
    //

    #if 0
    {
        RgDescriptorSetLayout *set_layouts[] = {
            engine->bind_group_layouts[BIND_GROUP_CAMERA],
            engine->bind_group_layouts[BIND_GROUP_MODEL],
        };
        RgPipelineLayoutInfo pipeline_layout_info = {};
        pipeline_layout_info.set_layouts = set_layouts;
        pipeline_layout_info.set_layout_count = sizeof(set_layouts) / sizeof(set_layouts[0]);

        engine->pipeline_layouts[PIPELINE_TYPE_MODEL] =
            rgPipelineLayoutCreate(device, &pipeline_layout_info);
    }

    {
        RgDescriptorSetLayout *set_layouts[] = {
            engine->bind_group_layouts[BIND_GROUP_POSTPROCESS],
        };
        RgPipelineLayoutInfo pipeline_layout_info = {};
        pipeline_layout_info.set_layouts = set_layouts;
        pipeline_layout_info.set_layout_count = sizeof(set_layouts) / sizeof(set_layouts[0]);

        engine->pipeline_layouts[PIPELINE_TYPE_POSTPROCESS] =
            rgPipelineLayoutCreate(device, &pipeline_layout_info);
    }
    #endif

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

    for (auto &slot : engine->pipeline_layout_map)
    {
        rgPipelineLayoutDestroy(device, slot.value);
    }

    for (auto &slot : engine->set_layout_map)
    {
        rgDescriptorSetLayoutDestroy(device, slot.value);
    }

    rgImageDestroy(device, engine->white_image);
    rgImageDestroy(device, engine->black_image);
    rgSamplerDestroy(device, engine->default_sampler);
    rgCmdPoolDestroy(device, engine->transfer_cmd_pool);

    engine->pipeline_layout_map.free();
    engine->set_layout_map.free();

    PlatformDestroy(engine->platform);

    ArenaDestroy(engine->arena);

    Free(engine->allocator, (void*)engine->exe_dir);
    Free(engine->allocator, engine);
}

Platform *EngineGetPlatform(Engine *engine)
{
    return engine->platform;
}

RgDescriptorSetLayout *EngineGetSetLayout(Engine *engine, const char *name)
{
    RgDescriptorSetLayout *set_layout = nullptr;
    engine->set_layout_map.get(name, &set_layout);
    assert(set_layout);
    return set_layout;
}

RgPipelineLayout *EngineGetPipelineLayout(Engine *engine, const char *name)
{
    RgPipelineLayout *pipeline_layout = nullptr;
    engine->pipeline_layout_map.get(name, &pipeline_layout);
    assert(pipeline_layout);
    return pipeline_layout;
}

const char *EngineGetExeDir(Engine *engine)
{
    return engine->exe_dir;
}

uint8_t *EngineLoadFileRelative(
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
    fread(data, *size, 1, f);

    fclose(f);

    Free(allocator, path);

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


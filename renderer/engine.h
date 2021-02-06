#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct RgPipelineLayout RgPipelineLayout;
typedef struct Platform Platform;
typedef struct Engine Engine;

typedef enum BindGroupType
{
    BIND_GROUP_CAMERA,
    BIND_GROUP_MODEL,
    BIND_GROUP_POSTPROCESS,
    BIND_GROUP_MAX,
} BindGroupType;

typedef enum PipelineType
{
    PIPELINE_TYPE_MODEL,
    PIPELINE_TYPE_POSTPROCESS,
    PIPELINE_TYPE_MAX,
} PipelineType;

Engine *EngineCreate(Allocator *allocator);
void EngineDestroy(Engine *engine);
Platform *EngineGetPlatform(Engine *engine);

RgDescriptorSetLayout *
EngineGetSetLayout(Engine *engine, BindGroupType type);

RgPipelineLayout *
EngineGetPipelineLayout(Engine *engine, PipelineType type);

const char *EngineGetExeDir(Engine *engine);

// Loads a file relative to the executable path
uint8_t *
EngineLoadFileRelative(Engine *engine, Allocator *allocator, const char *relative_path, size_t *size);

RgImage *EngineGetWhiteImage(Engine *engine);
RgImage *EngineGetBlackImage(Engine *engine);
RgSampler *EngineGetDefaultSampler(Engine *engine);

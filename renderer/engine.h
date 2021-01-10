#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct Platform Platform;
typedef struct Engine Engine;

typedef enum BindGroupType
{
    BIND_GROUP_CAMERA,
    BIND_GROUP_MODEL,
    BIND_GROUP_MAX,
} BindGroupType;

Engine *EngineCreate(Allocator *allocator);
void EngineDestroy(Engine *engine);
Platform *EngineGetPlatform(Engine *engine);

RgDescriptorSetLayout *
EngineGetSetLayout(Engine *engine, BindGroupType type);

const char *EngineGetExeDir(Engine *engine);

uint8_t *
EngineLoadFileRelative(Engine *engine, const char *relative_path, size_t *size);

RgImage *EngineGetWhiteImage(Engine *engine);
RgImage *EngineGetBlackImage(Engine *engine);
RgSampler *EngineGetDefaultSampler(Engine *engine);
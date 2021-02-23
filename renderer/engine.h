#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct RgPipelineLayout RgPipelineLayout;
typedef struct RgPipeline RgPipeline;
typedef struct Platform Platform;
typedef struct Engine Engine;

Engine *EngineCreate(Allocator *allocator, const char *spec, size_t spec_size);
void EngineDestroy(Engine *engine);
Platform *EngineGetPlatform(Engine *engine);

RgDescriptorSetLayout *
EngineGetSetLayout(Engine *engine, const char *name);

RgPipelineLayout *
EngineGetPipelineLayout(Engine *engine, const char *name);

const char *EngineGetExeDir(Engine *engine);

// Loads a file relative to the executable path
uint8_t *
EngineLoadFileRelative(Engine *engine, Allocator *allocator, const char *relative_path, size_t *size);

RgImage *EngineGetWhiteImage(Engine *engine);
RgImage *EngineGetBlackImage(Engine *engine);
RgSampler *EngineGetDefaultSampler(Engine *engine);
RgImage *EngineGetBRDFImage(Engine *engine);

RgPipeline *EngineCreateGraphicsPipeline(Engine *engine, const char *path, const char *type);
RgPipeline *EngineCreateComputePipeline(Engine *engine, const char *path, const char *type);

#ifdef __cplusplus
}
#endif

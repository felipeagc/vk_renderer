#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct RgBuffer RgBuffer;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct RgDescriptorSet RgDescriptorSet;
typedef struct RgPipelineLayout RgPipelineLayout;
typedef struct RgPipeline RgPipeline;
typedef struct RgBufferInfo RgBufferInfo;
typedef struct RgImageInfo RgImageInfo;
typedef struct RgSamplerInfo RgSamplerInfo;
typedef struct Platform Platform;
typedef struct Engine Engine;

typedef struct ImageHandle
{
	RgImage *image;
	uint32_t index;
} ImageHandle;

typedef struct SamplerHandle
{
	RgSampler *sampler;
	uint32_t index;
} SamplerHandle;

typedef struct BufferHandle
{
	RgBuffer *buffer;
	uint32_t index;
} BufferHandle;

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

RgPipeline *EngineCreateGraphicsPipeline2(Engine *engine, const char *path);

RgPipelineLayout *EngineGetGlobalPipelineLayout(Engine *engine);
RgDescriptorSet *EngineGetGlobalDescriptorSet(Engine *engine);

BufferHandle EngineAllocateStorageBufferHandle(Engine *engine, RgBufferInfo *info);
void EngineFreeStorageBufferHandle(Engine *engine, BufferHandle *handle);

ImageHandle EngineAllocateImageHandle(Engine *engine, RgImageInfo *info);
void EngineFreeImageHandle(Engine *engine, ImageHandle *handle);

SamplerHandle EngineAllocateSamplerHandle(Engine *engine, RgSamplerInfo *info);
void EngineFreeSamplerHandle(Engine *engine, SamplerHandle *handle);

#ifdef __cplusplus
}
#endif

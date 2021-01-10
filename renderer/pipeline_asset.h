#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct Platform Platform;
typedef struct RgPipeline RgPipeline;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct PipelineAsset PipelineAsset;

PipelineAsset *PipelineAssetCreateGraphics(
        Allocator *allocator,
        Platform *platform,
        const char *hlsl, size_t hlsl_size);

void PipelineAssetDestroy(PipelineAsset *pipeline_asset);

RgPipeline *PipelineAssetGetPipeline(PipelineAsset *pipeline_asset);

RgDescriptorSetLayout *PipelineAssetGetSetLayout(
        PipelineAsset *pipeline_asset, uint32_t index);

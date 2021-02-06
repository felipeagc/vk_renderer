#pragma once

#include <stdint.h>
#include <stddef.h>
#include "engine.h"

typedef struct Allocator Allocator;
typedef struct Engine Engine;
typedef struct RgPipeline RgPipeline;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct PipelineAsset PipelineAsset;

PipelineAsset *PipelineAssetCreateGraphics(
        Allocator *allocator,
        Engine *engine,
        PipelineType pipeline_type,
        const char *hlsl, size_t hlsl_size);

void PipelineAssetDestroy(PipelineAsset *pipeline_asset);

RgPipeline *PipelineAssetGetPipeline(PipelineAsset *pipeline_asset);

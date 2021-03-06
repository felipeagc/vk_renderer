#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct EgEngine EgEngine;
typedef struct EgAllocator EgAllocator;
typedef struct RgPipeline RgPipeline;
typedef struct RgPipelineLayout RgPipelineLayout;

#ifdef __cplusplus
extern "C" {
#endif

RgPipeline *egPipelineUtilCreateGraphicsPipeline(
        EgEngine *engine,
        EgAllocator *allocator,
        RgPipelineLayout *pipeline_layout,
        const char *hlsl, size_t hlsl_size);

#ifdef __cplusplus
}
#endif

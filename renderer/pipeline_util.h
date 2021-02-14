#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Engine Engine;
typedef struct Allocator Allocator;
typedef struct RgPipeline RgPipeline;
typedef struct RgPipelineLayout RgPipelineLayout;

#ifdef __cplusplus
extern "C" {
#endif

RgPipeline *PipelineUtilCreateGraphicsPipeline(
        Engine *engine,
        Allocator *allocator,
        RgPipelineLayout *pipeline_layout,
        const char *hlsl, size_t hlsl_size);

#ifdef __cplusplus
}
#endif

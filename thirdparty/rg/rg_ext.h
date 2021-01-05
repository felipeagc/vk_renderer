#ifndef RG_EXT_H
#define RG_EXT_H

#include "rg.h"

#ifdef __cplusplus
extern "C" {
#endif

RgPipeline *rgExtGraphicsPipelineCreateInferredBindings(
        RgDevice *device,
        bool dynamic_buffers,
        const RgGraphicsPipelineInfo *info);

RgPipeline *rgExtComputePipelineCreateInferredBindings(
        RgDevice *device,
        bool dynamic_buffers,
        const RgComputePipelineInfo *info);

#ifdef __cplusplus
}
#endif

#endif // RG_EXT_H

#include "pbr.h"

#include <assert.h>
#include <rg.h>
#include "allocator.h"
#include "engine.h"
#include "platform.h"
#include "pipeline_util.h"

extern "C" ImageHandle GenerateBRDFLUT(Engine *engine, RgCmdPool *cmd_pool, uint32_t dim)
{
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

    RgImageInfo info = {};
    info.extent = { dim, dim, 1 };
    info.format = RG_FORMAT_RG32_SFLOAT;
    info.aspect = RG_IMAGE_ASPECT_COLOR;
    info.usage = RG_IMAGE_USAGE_COLOR_ATTACHMENT | RG_IMAGE_USAGE_SAMPLED;
    info.sample_count = 1;
    info.mip_count = 1;
    info.layer_count = 1;
    ImageHandle image = EngineAllocateImageHandle(engine, &info);

    RgRenderPassInfo render_pass_info = {};
    render_pass_info.color_attachments = &image.image;
    render_pass_info.color_attachment_count = 1;
    RgRenderPass *render_pass = rgRenderPassCreate(device, &render_pass_info);

    RgPipeline *pipeline = EngineCreateGraphicsPipeline(engine, "../shaders/brdf.hlsl");

    RgCmdBuffer *cmd_buffer = rgCmdBufferCreate(device, cmd_pool);

    rgCmdBufferBegin(cmd_buffer);

    {
        RgClearValue clear_value = {};
        rgCmdSetRenderPass(cmd_buffer, render_pass, 1, &clear_value);

        rgCmdBindPipeline(cmd_buffer, pipeline);

        rgCmdDraw(cmd_buffer, 3, 1, 0, 0);
    }

    rgCmdBufferEnd(cmd_buffer);

    rgCmdBufferSubmit(cmd_buffer);
    rgCmdBufferWait(device, cmd_buffer);

    rgCmdBufferDestroy(device, cmd_pool, cmd_buffer);

    rgRenderPassDestroy(device, render_pass);
    rgPipelineDestroy(device, pipeline);
    
    return image;
}

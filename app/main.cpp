#include <stdio.h>
#include <assert.h>
#include <platform.hpp>
#include <camera.hpp>
#include <math.hpp>
#include <camera.hpp>

struct App
{
    Platform *platform;
    RgImage *offscreen_image;
    RgImage *offscreen_depth_image;
    RgRenderPass *offscreen_pass;

    RgCmdPool *cmd_pool;
    RgCmdBuffer *cmd_buffers[2];
    uint32_t current_frame;

    RgSampler *sampler;

    RgPipeline *offscreen_pipeline;
    RgPipeline *backbuffer_pipeline;

    RgDescriptorSet *descriptor_set;

    FPSCamera camera;
};

App *AppCreate();
void AppDestroy(App *app);
void AppResize(App *app);
void AppRenderFrame(App *app);
void AppRun(App *app);

App *AppCreate()
{
    App *app = new App();

    app->platform = PlatformCreate("App");

    RgDevice *device = PlatformGetDevice(app->platform);

    //
    // Create sampler
    //
    RgSamplerInfo sampler_info = {
        .anisotropy = false,
        .max_anisotropy = 0.0f,
        .min_lod = 0.0f,
        .max_lod = 1.0f,
        .mag_filter = RG_FILTER_LINEAR,
        .min_filter = RG_FILTER_LINEAR,
        .address_mode = RG_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .border_color = RG_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    app->sampler = rgSamplerCreate(device, &sampler_info);

    //
    // Offscreen pipeline
    //
    {
        size_t hlsl_size = 0;
        const char *hlsl = (const char*)
            PlatformLoadFileRelative(app->platform, "../shaders/color.hlsl", &hlsl_size);
        assert(hlsl);
        app->offscreen_pipeline = PlatformCreatePipeline(app->platform, hlsl, hlsl_size);
        delete[] hlsl;
    }

    //
    // Backbuffer pipeline
    //
    {
        size_t hlsl_size = 0;
        const char *hlsl = (const char*)
            PlatformLoadFileRelative(app->platform, "../shaders/post.hlsl", &hlsl_size);
        assert(hlsl);
        app->backbuffer_pipeline = PlatformCreatePipeline(app->platform, hlsl, hlsl_size);
        delete[] hlsl;
    }

    app->descriptor_set = rgPipelineCreateDescriptorSet(app->backbuffer_pipeline, 0);

    app->cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);
    app->cmd_buffers[0] = rgCmdBufferCreate(device, app->cmd_pool);
    app->cmd_buffers[1] = rgCmdBufferCreate(device, app->cmd_pool);

    AppResize(app);

    FPSCameraInit(&app->camera, app->platform);

    return app;
}

void AppDestroy(App *app)
{
    RgDevice *device = PlatformGetDevice(app->platform);

    rgPipelineDestroyDescriptorSet(app->backbuffer_pipeline, app->descriptor_set);

    rgPipelineDestroy(device, app->offscreen_pipeline);
    rgPipelineDestroy(device, app->backbuffer_pipeline);

    rgSamplerDestroy(device, app->sampler);

    rgImageDestroy(device, app->offscreen_image);
    rgImageDestroy(device, app->offscreen_depth_image);
    rgRenderPassDestroy(device, app->offscreen_pass);

    rgCmdBufferDestroy(device, app->cmd_pool, app->cmd_buffers[0]);
    rgCmdBufferDestroy(device, app->cmd_pool, app->cmd_buffers[1]);
    rgCmdPoolDestroy(device, app->cmd_pool);

    PlatformDestroy(app->platform);

    delete app;
}

void AppResize(App *app)
{
    uint32_t width, height;
    PlatformGetWindowSize(app->platform, &width, &height);

    RgDevice *device = PlatformGetDevice(app->platform);

    if (app->offscreen_pass)
    {
        rgRenderPassDestroy(device, app->offscreen_pass);
    }
    if (app->offscreen_image)
    {
        rgImageDestroy(device, app->offscreen_image);
    }
    if (app->offscreen_depth_image)
    {
        rgImageDestroy(device, app->offscreen_depth_image);
    }

    RgImageInfo offscreen_image_info = {
        .extent = { width, height, 1 },
        .format = RG_FORMAT_RGBA8_UNORM,
        .usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_COLOR_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_COLOR,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    app->offscreen_image = rgImageCreate(device, &offscreen_image_info);

    RgImageInfo offscreen_depth_image_info = {
        .extent = { width, height, 1 },
        .format = RG_FORMAT_D32_SFLOAT_S8_UINT,
        .usage = RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_DEPTH | RG_IMAGE_ASPECT_STENCIL,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    app->offscreen_depth_image = rgImageCreate(device, &offscreen_depth_image_info);

    RgRenderPassInfo render_pass_info = {
        .color_attachments = &app->offscreen_image,
        .color_attachment_count = 1,

        .depth_stencil_attachment = app->offscreen_depth_image,
    };
    app->offscreen_pass = rgRenderPassCreate(device, &render_pass_info);

    RgDescriptor descriptors[] = {
        { .image = {.image = app->offscreen_image} },
        { .image = {.sampler = app->sampler} },
    };

    rgUpdateDescriptorSet(app->descriptor_set, 2, descriptors);
}


void AppRenderFrame(App *app)
{
    RgSwapchain *swapchain = PlatformGetSwapchain(app->platform);
    RgCmdBuffer *cmd_buffer = app->cmd_buffers[app->current_frame];
    RgRenderPass *offscreen_pass = app->offscreen_pass;
    RgRenderPass *backbuffer_pass = rgSwapchainGetRenderPass(swapchain);

    rgSwapchainAcquireImage(swapchain);

    rgCmdBufferBegin(cmd_buffer);

    // Offscreen pass

    RgClearValue offscreen_clear_values[] = {
        {.color = { { 0.0, 0.0, 0.0, 1.0 } } },
        {.depth_stencil = { 0.0f, 0 } },
    };
    rgCmdSetRenderPass(cmd_buffer, offscreen_pass, 2, offscreen_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->offscreen_pipeline);

    rgCmdDraw(cmd_buffer, 3, 1, 0, 0);

    // Backbuffer pass

    RgClearValue backbuffer_clear_values[] = {
        {.color = { { 0.0, 0.0, 0.0, 1.0 } } },
    };
    rgCmdSetRenderPass(cmd_buffer, backbuffer_pass, 1, backbuffer_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->backbuffer_pipeline);
    rgCmdBindDescriptorSet(cmd_buffer, app->descriptor_set, 0, NULL);

    rgCmdDraw(cmd_buffer, 3, 1, 0, 0);

    rgCmdBufferEnd(cmd_buffer);

    rgCmdBufferWaitForPresent(cmd_buffer, swapchain);
    rgCmdBufferSubmit(cmd_buffer);

    rgSwapchainWaitForCommands(swapchain, cmd_buffer);
    rgSwapchainPresent(swapchain);

    app->current_frame = (app->current_frame + 1) % 2;
}

void AppRun(App *app)
{
    while (!PlatformShouldClose(app->platform))
    {
        PlatformPollEvents(app->platform);
        Event event = {};
        while (PlatformNextEvent(app->platform, &event))
        {
            switch (event.type)
            {
            case EVENT_WINDOW_RESIZED:
            {
                AppResize(app);
                break;
            }
            }
        }

        AppRenderFrame(app);
    }
}

int main()
{
    App *app = AppCreate();
    AppRun(app);
    AppDestroy(app);

    return 0;
}

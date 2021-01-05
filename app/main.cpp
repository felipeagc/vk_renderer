#include <stdio.h>
#include <assert.h>
#include <platform.hpp>

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

    static App *create()
    {
        App *app = new App();

        app->platform = Platform::create("App");

        RgDevice *device = app->platform->getDevice();

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
                app->platform->loadFileRelative("../shaders/color.hlsl", &hlsl_size);
            assert(hlsl);
            app->offscreen_pipeline = app->platform->createPipeline(hlsl, hlsl_size);
            delete[] hlsl;
        }

        //
        // Backbuffer pipeline
        //
        {
            size_t hlsl_size = 0;
            const char *hlsl = (const char*)
                app->platform->loadFileRelative("../shaders/post.hlsl", &hlsl_size);
            assert(hlsl);
            app->backbuffer_pipeline = app->platform->createPipeline(hlsl, hlsl_size);
            delete[] hlsl;
        }

        app->descriptor_set = rgPipelineCreateDescriptorSet(app->backbuffer_pipeline, 0);

        app->cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);
        app->cmd_buffers[0] = rgCmdBufferCreate(device, app->cmd_pool);
        app->cmd_buffers[1] = rgCmdBufferCreate(device, app->cmd_pool);

        app->resize();

        return app;
    }

    void resize()
    {
        uint32_t width, height;
        this->platform->getWindowSize(&width, &height);

        RgDevice *device = this->platform->getDevice();

        if (this->offscreen_pass)
        {
            rgRenderPassDestroy(device, this->offscreen_pass);
        }
        if (this->offscreen_image)
        {
            rgImageDestroy(device, this->offscreen_image);
        }
        if (this->offscreen_depth_image)
        {
            rgImageDestroy(device, this->offscreen_depth_image);
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
        this->offscreen_image = rgImageCreate(device, &offscreen_image_info);

        RgImageInfo offscreen_depth_image_info = {
            .extent = { width, height, 1 },
            .format = RG_FORMAT_D32_SFLOAT_S8_UINT,
            .usage = RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .aspect = RG_IMAGE_ASPECT_DEPTH | RG_IMAGE_ASPECT_STENCIL,
            .sample_count = 1,
            .mip_count = 1,
            .layer_count = 1,
        };
        this->offscreen_depth_image = rgImageCreate(device, &offscreen_depth_image_info);

        RgRenderPassInfo render_pass_info = {
            .color_attachments = &this->offscreen_image,
            .color_attachment_count = 1,

            .depth_stencil_attachment = this->offscreen_depth_image,
        };
        this->offscreen_pass = rgRenderPassCreate(device, &render_pass_info);

        RgDescriptor descriptors[] = {
            { .image = {.image = this->offscreen_image} },
            { .image = {.sampler = this->sampler} },
        };

        rgUpdateDescriptorSet(this->descriptor_set, 2, descriptors);
    }

    void destroy()
    {
        RgDevice *device = this->platform->getDevice();

        rgPipelineDestroyDescriptorSet(this->backbuffer_pipeline, this->descriptor_set);

        rgPipelineDestroy(device, this->offscreen_pipeline);
        rgPipelineDestroy(device, this->backbuffer_pipeline);

        rgSamplerDestroy(device, this->sampler);

        rgImageDestroy(device, this->offscreen_image);
        rgImageDestroy(device, this->offscreen_depth_image);
        rgRenderPassDestroy(device, this->offscreen_pass);

        rgCmdBufferDestroy(device, this->cmd_pool, this->cmd_buffers[0]);
        rgCmdBufferDestroy(device, this->cmd_pool, this->cmd_buffers[1]);
        rgCmdPoolDestroy(device, this->cmd_pool);

        this->platform->destroy();

        delete this;
    }

    void renderFrame()
    {
        RgSwapchain *swapchain = this->platform->getSwapchain();
        RgCmdBuffer *cmd_buffer = this->cmd_buffers[this->current_frame];
        RgRenderPass *offscreen_pass = this->offscreen_pass;
        RgRenderPass *backbuffer_pass = rgSwapchainGetRenderPass(swapchain);

        rgSwapchainAcquireImage(swapchain);

        rgCmdBufferBegin(cmd_buffer);

        // Offscreen pass

        RgClearValue offscreen_clear_values[] = {
            {.color = { { 0.0, 0.0, 0.0, 1.0 } } },
            {.depth_stencil = { 1.0f, 0 } },
        };
        rgCmdSetRenderPass(cmd_buffer, offscreen_pass, 2, offscreen_clear_values);

        rgCmdBindPipeline(cmd_buffer, this->offscreen_pipeline);

        rgCmdDraw(cmd_buffer, 3, 1, 0, 0);

        // Backbuffer pass

        RgClearValue backbuffer_clear_values[] = {
            {.color = { { 0.0, 0.0, 0.0, 1.0 } } },
        };
        rgCmdSetRenderPass(cmd_buffer, backbuffer_pass, 1, backbuffer_clear_values);

        rgCmdBindPipeline(cmd_buffer, this->backbuffer_pipeline);
        rgCmdBindDescriptorSet(cmd_buffer, this->descriptor_set, 0, NULL);

        rgCmdDraw(cmd_buffer, 3, 1, 0, 0);

        rgCmdBufferEnd(cmd_buffer);

        rgCmdBufferWaitForPresent(cmd_buffer, swapchain);
        rgCmdBufferSubmit(cmd_buffer);

        rgSwapchainWaitForCommands(swapchain, cmd_buffer);
        rgSwapchainPresent(swapchain);

        this->current_frame = (this->current_frame + 1) % 2;
    }

    void run()
    {
        while (!platform->shouldClose())
        {
            platform->pollEvents();
            Event event = {};
            while (platform->nextEvent(&event))
            {
                switch (event.type)
                {
                case EVENT_WINDOW_RESIZED:
                {
                    this->resize();
                    break;
                }
                }
            }

            this->renderFrame();
        }
    }
};

int main()
{
    App *app = App::create();
    app->run();
    app->destroy();

    return 0;
}

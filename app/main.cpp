#include <stdio.h>
#include <assert.h>
#include <renderer/platform.h>
#include <renderer/camera.h>
#include <renderer/math.h>
#include <renderer/mesh.h>
#include <renderer/allocator.h>
#include <renderer/uniform_arena.h>
#include <renderer/pipeline_asset.h>

struct App
{
    Platform *platform;
    RgImage *offscreen_image;
    RgImage *offscreen_depth_image;
    RgRenderPass *offscreen_pass;

    RgCmdPool *cmd_pool;
    RgCmdBuffer *cmd_buffers[2];
    uint32_t current_frame;

    double last_time;
    double delta_time;

    RgSampler *sampler;

    PipelineAsset *offscreen_pipeline;
    PipelineAsset *backbuffer_pipeline;

    RgDescriptorSet *camera_set;

    RgDescriptorSet *descriptor_set;

    UniformArena *uniform_arena;
    FPSCamera camera;
    Mesh *cube_mesh;
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
        app->offscreen_pipeline = PipelineAssetCreateGraphics(
                NULL, app->platform, hlsl, hlsl_size);
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
        app->backbuffer_pipeline = PipelineAssetCreateGraphics(
                NULL, app->platform, hlsl, hlsl_size);
        delete[] hlsl;
    }

    app->cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);
    app->cmd_buffers[0] = rgCmdBufferCreate(device, app->cmd_pool);
    app->cmd_buffers[1] = rgCmdBufferCreate(device, app->cmd_pool);

    FPSCameraInit(&app->camera, app->platform);

    app->uniform_arena = UniformArenaCreate(app->platform, NULL);
    app->cube_mesh = MeshCreateUVSphere(app->platform, app->cmd_pool, NULL, 1.0f, 16);
    app->last_time = PlatformGetTime(app->platform);

    AppResize(app);

    return app;
}

void AppDestroy(App *app)
{
    RgDevice *device = PlatformGetDevice(app->platform);

    UniformArenaDestroy(app->uniform_arena);
    MeshDestroy(app->cube_mesh);

    rgDescriptorSetDestroy(device, app->camera_set);
    rgDescriptorSetDestroy(device, app->descriptor_set);

    PipelineAssetDestroy(app->offscreen_pipeline);
    PipelineAssetDestroy(app->backbuffer_pipeline);

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

    {
        if (app->camera_set)
        {
            rgDescriptorSetDestroy(device, app->camera_set);
            app->camera_set = NULL;
        }

        RgDescriptorSetEntry entries[] = {
            { .binding = 0, .buffer = UniformArenaGetBuffer(app->uniform_arena) }
        };

        RgDescriptorSetInfo info = {
            PipelineAssetGetSetLayout(app->offscreen_pipeline, 0), // layout
            entries, // entries
            sizeof(entries) / sizeof(entries[0]), // entry_count
        };

        app->camera_set = rgDescriptorSetCreate(device, &info);
    }

    {
        if (app->descriptor_set)
        {
            rgDescriptorSetDestroy(device, app->descriptor_set);
            app->descriptor_set = NULL;
        }

        RgDescriptorSetEntry entries[] = {
            { .binding = 0, .image = app->offscreen_image },
            { .binding = 1, .sampler = app->sampler },
        };

        RgDescriptorSetInfo info = {
            PipelineAssetGetSetLayout(app->backbuffer_pipeline, 0), // layout
            entries, // entries
            sizeof(entries) / sizeof(entries[0]), // entry_count
        };

        app->descriptor_set = rgDescriptorSetCreate(device, &info);
    }
}

void AppRenderFrame(App *app)
{
    CameraUniform camera_uniform = FPSCameraUpdate(&app->camera, app->delta_time);

    RgSwapchain *swapchain = PlatformGetSwapchain(app->platform);
    RgCmdBuffer *cmd_buffer = app->cmd_buffers[app->current_frame];
    RgRenderPass *offscreen_pass = app->offscreen_pass;
    RgRenderPass *backbuffer_pass = rgSwapchainGetRenderPass(swapchain);

    rgSwapchainAcquireImage(swapchain);

    rgCmdBufferBegin(cmd_buffer);

    // Offscreen pass

    RgClearValue offscreen_clear_values[2];
    offscreen_clear_values[0].color = { { 0.0, 0.0, 0.0, 1.0 } };
    offscreen_clear_values[1].depth_stencil = { 0.0f, 0 };
    rgCmdSetRenderPass(cmd_buffer, offscreen_pass, 2, offscreen_clear_values);

    rgCmdBindPipeline(cmd_buffer, PipelineAssetGetPipeline(app->offscreen_pipeline));
    uint32_t uniform_offset;
    void *uniform_ptr = UniformArenaUse(app->uniform_arena, &uniform_offset, sizeof(camera_uniform));
    memcpy(uniform_ptr, &camera_uniform, sizeof(camera_uniform));
    rgCmdBindDescriptorSet(cmd_buffer, 0, app->camera_set, 1, &uniform_offset);

    rgCmdBindVertexBuffer(
            cmd_buffer, MeshGetVertexBuffer(app->cube_mesh), 0);
    rgCmdBindIndexBuffer(
            cmd_buffer, MeshGetIndexBuffer(app->cube_mesh), 0, RG_INDEX_TYPE_UINT32);

    rgCmdDrawIndexed(cmd_buffer, MeshGetIndexCount(app->cube_mesh), 1, 0, 0, 0);

    // Backbuffer pass

    RgClearValue backbuffer_clear_values[1];
    backbuffer_clear_values[0].color = { { 0.0, 0.0, 0.0, 1.0 } };
    rgCmdSetRenderPass(cmd_buffer, backbuffer_pass, 1, backbuffer_clear_values);

    rgCmdBindPipeline(cmd_buffer, PipelineAssetGetPipeline(app->backbuffer_pipeline));
    rgCmdBindDescriptorSet(cmd_buffer, 0, app->descriptor_set, 0, NULL);

    rgCmdDraw(cmd_buffer, 3, 1, 0, 0);

    rgCmdBufferEnd(cmd_buffer);

    rgCmdBufferWaitForPresent(cmd_buffer, swapchain);
    rgCmdBufferSubmit(cmd_buffer);

    rgSwapchainWaitForCommands(swapchain, cmd_buffer);
    rgSwapchainPresent(swapchain);

    app->current_frame = (app->current_frame + 1) % 2;
    if (app->current_frame == 0)
    {
        UniformArenaReset(app->uniform_arena);
    }
}

void AppRun(App *app)
{
    while (!PlatformShouldClose(app->platform))
    {
        PlatformPollEvents(app->platform);

        double now = PlatformGetTime(app->platform);
        app->delta_time = now - app->last_time;
        app->last_time = now;

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
            case EVENT_KEY_PRESSED:
            {
                if (event.keyboard.key == KEY_ESCAPE)
                {
                    PlatformSetCursorEnabled(
                            app->platform,
                            !PlatformGetCursorEnabled(app->platform));
                }
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

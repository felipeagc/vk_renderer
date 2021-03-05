#include <stdio.h>
#include <renderer/engine.h>
#include <renderer/camera.h>
#include <renderer/math.h>
#include <renderer/mesh.h>
#include <renderer/allocator.h>
#include <renderer/model_asset.h>

struct App
{
    Engine *engine;
    ImageHandle offscreen_image;
    RgImage *offscreen_depth_image;
    RgRenderPass *offscreen_pass;

    RgCmdPool *cmd_pool;
    RgCmdBuffer *cmd_buffers[2];
    uint32_t current_frame;

    double last_time;
    double delta_time;

    SamplerHandle sampler;

    RgPipeline *offscreen_pipeline;
    RgPipeline *backbuffer_pipeline;

    ModelManager *model_manager;
    FPSCamera camera;
    ModelAsset *model_asset;
    Mesh *cube_mesh;
    ModelAsset *gltf_asset;
};

App *AppCreate();
void AppDestroy(App *app);
void AppResize(App *app);
void AppRenderFrame(App *app);
void AppRun(App *app);

App *AppCreate()
{
    App *app = new App();

    app->engine = EngineCreate(NULL);

    RgDevice *device = EngineGetDevice(app->engine);

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
    app->sampler = EngineAllocateSamplerHandle(app->engine, &sampler_info);

    //
    // Offscreen pipeline
    //
    app->offscreen_pipeline =
        EngineCreateGraphicsPipeline(app->engine, "../shaders/color.hlsl");
    EG_ASSERT(app->offscreen_pipeline);

    //
    // Backbuffer pipeline
    //
    app->backbuffer_pipeline =
        EngineCreateGraphicsPipeline(app->engine, "../shaders/post.hlsl");
    EG_ASSERT(app->backbuffer_pipeline);

    app->cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);
    app->cmd_buffers[0] = rgCmdBufferCreate(device, app->cmd_pool);
    app->cmd_buffers[1] = rgCmdBufferCreate(device, app->cmd_pool);

    FPSCameraInit(&app->camera, app->engine);

    app->model_manager = ModelManagerCreate(NULL, app->engine, 256, 256);
    app->cube_mesh = MeshCreateUVSphere(NULL, app->engine, app->cmd_pool, 1.0f, 16);
    app->last_time = EngineGetTime(app->engine);

    app->model_asset = ModelAssetFromMesh(app->model_manager, app->cube_mesh);

    size_t gltf_data_size = 0;
    uint8_t *gltf_data = EngineLoadFileRelative(
        app->engine, NULL, "../assets/helmet.glb", &gltf_data_size);
    EG_ASSERT(gltf_data);
    app->gltf_asset = ModelAssetFromGltf(app->model_manager, gltf_data, gltf_data_size);
    Free(NULL, gltf_data);

    AppResize(app);

    return app;
}

void AppDestroy(App *app)
{
    RgDevice *device = EngineGetDevice(app->engine);

    ModelAssetDestroy(app->gltf_asset);
    ModelAssetDestroy(app->model_asset);
    MeshDestroy(app->cube_mesh);
    ModelManagerDestroy(app->model_manager);

    rgPipelineDestroy(device, app->offscreen_pipeline);
    rgPipelineDestroy(device, app->backbuffer_pipeline);

    EngineFreeSamplerHandle(app->engine, &app->sampler);

    EngineFreeImageHandle(app->engine, &app->offscreen_image);
    rgImageDestroy(device, app->offscreen_depth_image);
    rgRenderPassDestroy(device, app->offscreen_pass);

    rgCmdBufferDestroy(device, app->cmd_pool, app->cmd_buffers[0]);
    rgCmdBufferDestroy(device, app->cmd_pool, app->cmd_buffers[1]);
    rgCmdPoolDestroy(device, app->cmd_pool);

    EngineDestroy(app->engine);

    delete app;
}

void AppResize(App *app)
{
    RgDevice *device = EngineGetDevice(app->engine);

    uint32_t width, height;
    EngineGetWindowSize(app->engine, &width, &height);

    if (app->offscreen_pass)
    {
        rgRenderPassDestroy(device, app->offscreen_pass);
    }
    if (app->offscreen_image.image)
    {
        EngineFreeImageHandle(app->engine, &app->offscreen_image);
    }
    if (app->offscreen_depth_image)
    {
        rgImageDestroy(device, app->offscreen_depth_image);
    }

    RgImageInfo offscreen_image_info = {
        .extent = {width, height, 1},
        .format = RG_FORMAT_RGBA8_UNORM,
        .usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_COLOR_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_COLOR,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    app->offscreen_image = EngineAllocateImageHandle(app->engine, &offscreen_image_info);

    RgImageInfo offscreen_depth_image_info = {
        .extent = {width, height, 1},
        .format = RG_FORMAT_D32_SFLOAT_S8_UINT,
        .usage = RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_DEPTH | RG_IMAGE_ASPECT_STENCIL,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    app->offscreen_depth_image = rgImageCreate(device, &offscreen_depth_image_info);

    RgRenderPassInfo render_pass_info = {
        .color_attachments = &app->offscreen_image.image,
        .color_attachment_count = 1,

        .depth_stencil_attachment = app->offscreen_depth_image,
    };
    app->offscreen_pass = rgRenderPassCreate(device, &render_pass_info);
}

void AppRenderFrame(App *app)
{
    CameraUniform camera_uniform = FPSCameraUpdate(&app->camera, (float)app->delta_time);

    RgSwapchain *swapchain = EngineGetSwapchain(app->engine);
    RgCmdBuffer *cmd_buffer = app->cmd_buffers[app->current_frame];
    RgRenderPass *offscreen_pass = app->offscreen_pass;
    RgRenderPass *backbuffer_pass = rgSwapchainGetRenderPass(swapchain);

    rgSwapchainAcquireImage(swapchain);

    rgCmdBufferBegin(cmd_buffer);

    // Offscreen pass

    RgClearValue offscreen_clear_values[2];
    offscreen_clear_values[0].color = {{0.0, 0.0, 0.0, 1.0}};
    offscreen_clear_values[1].depth_stencil = {0.0f, 0};
    rgCmdSetRenderPass(cmd_buffer, offscreen_pass, 2, offscreen_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->offscreen_pipeline);
    rgCmdBindDescriptorSet(
        cmd_buffer, 0, EngineGetGlobalDescriptorSet(app->engine), 0, NULL);

    ModelManagerBeginFrame(app->model_manager, &camera_uniform);

    {
        float4x4 transform = eg_float4x4_diagonal(1.0f);
        eg_float4x4_translate(&transform, V3(-2.0, 0.0, -2.0));
        ModelAssetRender(app->model_asset, cmd_buffer, &transform);
    }

    {
        float4x4 transform = eg_float4x4_diagonal(1.0f);
        eg_float4x4_translate(&transform, V3(0.0, 0.0, -2.0));
        ModelAssetRender(app->model_asset, cmd_buffer, &transform);
    }

    {
        float4x4 transform = eg_float4x4_diagonal(1.0f);
        eg_float4x4_translate(&transform, V3(2.0, 0.0, -2.0));
        ModelAssetRender(app->gltf_asset, cmd_buffer, &transform);
    }

    // Backbuffer pass

    RgClearValue backbuffer_clear_values[1];
    backbuffer_clear_values[0].color = {{0.0, 0.0, 0.0, 1.0}};
    rgCmdSetRenderPass(cmd_buffer, backbuffer_pass, 1, backbuffer_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->backbuffer_pipeline);
    rgCmdBindDescriptorSet(
        cmd_buffer, 0, EngineGetGlobalDescriptorSet(app->engine), 0, NULL);

    struct
    {
        uint32_t offscreen_image_index;
        uint32_t sampler_index;
    } pc;
    pc.offscreen_image_index = app->offscreen_image.index;
    pc.sampler_index = app->sampler.index;
    rgCmdPushConstants(cmd_buffer, 0, sizeof(pc), &pc);

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
    while (!EngineShouldClose(app->engine))
    {
        EnginePollEvents(app->engine);

        double now = EngineGetTime(app->engine);
        app->delta_time = now - app->last_time;
        app->last_time = now;

        Event event = {};
        while (EngineNextEvent(app->engine, &event))
        {
            switch (event.type)
            {
            case EVENT_WINDOW_RESIZED: {
                AppResize(app);
                break;
            }
            case EVENT_KEY_PRESSED: {
                if (event.keyboard.key == KEY_ESCAPE)
                {
                    EngineSetCursorEnabled(
                        app->engine, !EngineGetCursorEnabled(app->engine));
                }
                break;
            }

            default: break;
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

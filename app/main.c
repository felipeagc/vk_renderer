#include <stdio.h>
#include <renderer/engine.h>
#include <renderer/camera.h>
#include <renderer/math.h>
#include <renderer/mesh.h>
#include <renderer/allocator.h>
#include <renderer/model_asset.h>

typedef struct App
{
    EgEngine *engine;
    EgImage offscreen_image;
    EgImage offscreen_depth_image;
    RgRenderPass *offscreen_pass;

    EgImage pingpong_images[2];
    RgRenderPass *pingpong_renderpasses[2];

    RgCmdPool *cmd_pool;
    RgCmdBuffer *cmd_buffers[2];
    uint32_t current_frame;

    double last_time;
    double delta_time;

    EgSampler sampler;

    RgPipeline *offscreen_pipeline;
    RgPipeline *backbuffer_pipeline;
    RgPipeline *blur_pipeline;

    EgModelManager *model_manager;
    EgFPSCamera camera;
    EgModelAsset *model_asset;
    EgMesh *cube_mesh;
    EgModelAsset *gltf_asset;
} App;

App *appCreate();
void appDestroy(App *app);
void appResize(App *app);
void appRenderFrame(App *app);
void appRun(App *app);

App *appCreate()
{
    App *app = egAllocate(NULL, sizeof(App));
    *app = (App){};

    app->engine = egEngineCreate(NULL);

    RgDevice *device = egEngineGetDevice(app->engine);

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
    app->sampler = egEngineAllocateSampler(app->engine, &sampler_info);

    //
    // Offscreen pipeline
    //
    app->offscreen_pipeline =
        egEngineCreateGraphicsPipeline(app->engine, "../shaders/color.hlsl");
    EG_ASSERT(app->offscreen_pipeline);

    //
    // Backbuffer pipeline
    //
    app->backbuffer_pipeline =
        egEngineCreateGraphicsPipeline(app->engine, "../shaders/post.hlsl");
    EG_ASSERT(app->backbuffer_pipeline);

    app->blur_pipeline =
        egEngineCreateGraphicsPipeline(app->engine, "../shaders/blur.hlsl");
    EG_ASSERT(app->blur_pipeline);

    app->cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);
    app->cmd_buffers[0] = rgCmdBufferCreate(device, app->cmd_pool);
    app->cmd_buffers[1] = rgCmdBufferCreate(device, app->cmd_pool);

    egFPSCameraInit(&app->camera, app->engine);

    app->model_manager = egModelManagerCreate(NULL, app->engine, 256, 256);
    app->cube_mesh = egMeshCreateUVSphere(NULL, app->engine, app->cmd_pool, 1.0f, 16);
    app->last_time = egEngineGetTime(app->engine);

    app->model_asset = egModelAssetFromMesh(app->model_manager, app->cube_mesh);

    size_t gltf_data_size = 0;
    uint8_t *gltf_data = egEngineLoadFileRelative(
        app->engine, NULL, "../assets/helmet.glb", &gltf_data_size);
    EG_ASSERT(gltf_data);
    app->gltf_asset = egModelAssetFromGltf(app->model_manager, gltf_data, gltf_data_size);
    egFree(NULL, gltf_data);

    appResize(app);

    return app;
}

void appDestroy(App *app)
{
    RgDevice *device = egEngineGetDevice(app->engine);

    egModelAssetDestroy(app->gltf_asset);
    egModelAssetDestroy(app->model_asset);
    egMeshDestroy(app->cube_mesh);
    egModelManagerDestroy(app->model_manager);

    rgPipelineDestroy(device, app->offscreen_pipeline);
    rgPipelineDestroy(device, app->backbuffer_pipeline);
    rgPipelineDestroy(device, app->blur_pipeline);

    egEngineFreeSampler(app->engine, &app->sampler);

    for (size_t i = 0; i < EG_CARRAY_LENGTH(app->pingpong_renderpasses); ++i)
    {
        rgRenderPassDestroy(device, app->pingpong_renderpasses[i]);
    }
    for (size_t i = 0; i < EG_CARRAY_LENGTH(app->pingpong_images); ++i)
    {
        egEngineFreeImage(app->engine, &app->pingpong_images[i]);
    }

    egEngineFreeImage(app->engine, &app->offscreen_image);
    egEngineFreeImage(app->engine, &app->offscreen_depth_image);
    rgRenderPassDestroy(device, app->offscreen_pass);

    rgCmdBufferDestroy(device, app->cmd_pool, app->cmd_buffers[0]);
    rgCmdBufferDestroy(device, app->cmd_pool, app->cmd_buffers[1]);
    rgCmdPoolDestroy(device, app->cmd_pool);

    egEngineDestroy(app->engine);

    egFree(NULL, app);
}

void appResize(App *app)
{
    RgDevice *device = egEngineGetDevice(app->engine);

    uint32_t width, height;
    egEngineGetWindowSize(app->engine, &width, &height);

    if (app->offscreen_pass)
    {
        rgRenderPassDestroy(device, app->offscreen_pass);
        app->offscreen_pass = NULL;
    }
    if (app->offscreen_image.image)
    {
        egEngineFreeImage(app->engine, &app->offscreen_image);
    }
    if (app->offscreen_depth_image.image)
    {
        egEngineFreeImage(app->engine, &app->offscreen_depth_image);
    }

    for (size_t i = 0; i < EG_CARRAY_LENGTH(app->pingpong_renderpasses); ++i)
    {
        if (app->pingpong_renderpasses[i])
        {
            rgRenderPassDestroy(device, app->pingpong_renderpasses[i]);
            app->pingpong_renderpasses[i] = NULL;
        }
    }
    for (size_t i = 0; i < EG_CARRAY_LENGTH(app->pingpong_images); ++i)
    {
        if (app->pingpong_images[i].image)
        {
            egEngineFreeImage(app->engine, &app->pingpong_images[i]);
        }
    }

    RgImageInfo offscreen_image_info = {
        .extent = {width, height, 1},
        .format = RG_FORMAT_RGBA16_SFLOAT,
        .usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_COLOR_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_COLOR,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    app->offscreen_image = egEngineAllocateImage(app->engine, &offscreen_image_info);

    RgImageInfo offscreen_depth_image_info = {
        .extent = {width, height, 1},
        .format = RG_FORMAT_D32_SFLOAT_S8_UINT,
        .usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_DEPTH,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    app->offscreen_depth_image =
        egEngineAllocateImage(app->engine, &offscreen_depth_image_info);

    RgImageInfo pingpong_image_info = {
        .extent = {width, height, 1},
        .format = RG_FORMAT_RGBA16_SFLOAT,
        .usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_COLOR_ATTACHMENT,
        .aspect = RG_IMAGE_ASPECT_COLOR,
        .sample_count = 1,
        .mip_count = 1,
        .layer_count = 1,
    };
    for (size_t i = 0; i < EG_CARRAY_LENGTH(app->pingpong_images); ++i)
    {
        app->pingpong_images[i] =
            egEngineAllocateImage(app->engine, &pingpong_image_info);

        app->pingpong_renderpasses[i] = rgRenderPassCreate(
            device,
            &(RgRenderPassInfo){
                .color_attachments = (RgImage *[]){app->pingpong_images[i].image},
                .color_attachment_count = 1,
            });
    }

    RgRenderPassInfo render_pass_info = {
        .color_attachments =
            (RgImage *[]){
                app->offscreen_image.image,
                app->pingpong_images[0].image,
            },
        .color_attachment_count = 2,

        .depth_stencil_attachment = app->offscreen_depth_image.image,
    };
    app->offscreen_pass = rgRenderPassCreate(device, &render_pass_info);
}

void appRenderFrame(App *app)
{
    EgCameraUniform camera_uniform =
        egFPSCameraUpdate(&app->camera, (float)app->delta_time);

    RgSwapchain *swapchain = egEngineGetSwapchain(app->engine);
    RgCmdBuffer *cmd_buffer = app->cmd_buffers[app->current_frame];
    RgRenderPass *offscreen_pass = app->offscreen_pass;
    RgRenderPass *backbuffer_pass = rgSwapchainGetRenderPass(swapchain);

    rgSwapchainAcquireImage(swapchain);

    rgCmdBufferBegin(cmd_buffer);

    // Offscreen pass

    RgClearValue offscreen_clear_values[] = {
        {.color = {{0.0, 0.0, 0.0, 1.0}}},
        {.color = {{0.0, 0.0, 0.0, 1.0}}},
        {.depth_stencil = {0.0f, 0}},
    };
    rgCmdSetRenderPass(
        cmd_buffer,
        offscreen_pass,
        EG_CARRAY_LENGTH(offscreen_clear_values),
        offscreen_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->offscreen_pipeline);
    rgCmdBindDescriptorSet(
        cmd_buffer, 0, egEngineGetGlobalDescriptorSet(app->engine), 0, NULL);

    egModelManagerBeginFrame(app->model_manager, &camera_uniform);

    {
        float4x4 transform = egFloat4x4Diagonal(1.0f);
        egFloat4x4Rotate(&transform, (float)egEngineGetTime(app->engine) / 100.0f, V3(0, 1, 0));
        egFloat4x4Translate(&transform, V3(-3.0, 0.0, -3.0));
        egModelAssetRender(app->model_asset, cmd_buffer, &transform);
    }

    {
        float4x4 transform = egFloat4x4Diagonal(1.0f);
        egFloat4x4Rotate(&transform, (float)egEngineGetTime(app->engine) / 100.0f, V3(0, 1, 0));
        egFloat4x4Translate(&transform, V3(0.0, 0.0, -3.0));
        egModelAssetRender(app->gltf_asset, cmd_buffer, &transform);
    }

    {
        float4x4 transform = egFloat4x4Diagonal(1.0f);
        egFloat4x4Rotate(&transform, (float)egEngineGetTime(app->engine) / 100.0f, V3(0, 1, 0));
        egFloat4x4Translate(&transform, V3(3.0, 0.0, -3.0));
        egModelAssetRender(app->model_asset, cmd_buffer, &transform);
    }

    // Blur pass

    {
        for (uint32_t i = 0; i < 10; ++i)
        {
            struct
            {
                uint32_t image_index;
                uint32_t sampler_index;

                uint32_t horizontal;
            } pc;

            pc.horizontal = (i + 1) % 2;
            pc.image_index = app->pingpong_images[i%2].index;
            pc.sampler_index = app->sampler.index;

            RgRenderPass *blur_renderpass = app->pingpong_renderpasses[(i+1) % 2];

            RgClearValue clear_values[] = {
                {.color = {{0.0, 0.0, 0.0, 1.0}}},
            };
            rgCmdSetRenderPass(
                cmd_buffer,
                blur_renderpass,
                EG_CARRAY_LENGTH(clear_values),
                clear_values);

            rgCmdBindPipeline(cmd_buffer, app->blur_pipeline);
            rgCmdBindDescriptorSet(
                cmd_buffer, 0, egEngineGetGlobalDescriptorSet(app->engine), 0, NULL);

            rgCmdPushConstants(cmd_buffer, 0, sizeof(pc), &pc);

            rgCmdDraw(cmd_buffer, 3, 1, 0, 0);
        }
    }

    // Backbuffer pass

    RgClearValue backbuffer_clear_values[1];
    backbuffer_clear_values[0].color = (RgClearColorValue){{0.0, 0.0, 0.0, 1.0}};
    rgCmdSetRenderPass(cmd_buffer, backbuffer_pass, 1, backbuffer_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->backbuffer_pipeline);
    rgCmdBindDescriptorSet(
        cmd_buffer, 0, egEngineGetGlobalDescriptorSet(app->engine), 0, NULL);

    struct
    {
        uint32_t offscreen_image_index;
        uint32_t bloom_image_index;
        uint32_t sampler_index;
    } pc;
    pc.offscreen_image_index = app->offscreen_image.index;
    pc.bloom_image_index =
        app->pingpong_images[EG_CARRAY_LENGTH(app->pingpong_images) - 1].index;
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

void appRun(App *app)
{
    while (!egEngineShouldClose(app->engine))
    {
        egEnginePollEvents(app->engine);

        double now = egEngineGetTime(app->engine);
        app->delta_time = now - app->last_time;
        app->last_time = now;

        EgEvent event = {};
        while (egEngineNextEvent(app->engine, &event))
        {
            switch (event.type)
            {
            case EVENT_WINDOW_RESIZED: {
                appResize(app);
                break;
            }
            case EVENT_KEY_PRESSED: {
                if (event.keyboard.key == EG_KEY_ESCAPE)
                {
                    egEngineSetCursorEnabled(
                        app->engine, !egEngineGetCursorEnabled(app->engine));
                }
                break;
            }

            default: break;
            }
        }

        appRenderFrame(app);
    }
}

int main()
{
    App *app = appCreate();
    appRun(app);
    appDestroy(app);

    return 0;
}

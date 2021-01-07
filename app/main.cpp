#include <stdio.h>
#include <assert.h>
#include <renderer/platform.h>
#include <renderer/camera.h>
#include <renderer/math.h>
#include <renderer/mesh.h>
#include <renderer/allocator.h>

struct UniformBuffer
{
    Platform *platform;
    Allocator *allocator;
    RgBuffer *buffer;
    uint8_t *mapping;
    size_t size;
    size_t offset;
    size_t alignment;
};

static inline size_t alignTo(size_t n, size_t to)
{
    size_t rest = n % to;
    return (rest != 0) ? (n + to - rest) : n;
}

UniformBuffer *UniformBufferCreate(Platform *platform, Allocator *allocator)
{
    UniformBuffer *uniform_buffer =
        (UniformBuffer*)Allocate(allocator, sizeof(UniformBuffer));
    *uniform_buffer = {};

    uniform_buffer->platform = platform;
    uniform_buffer->allocator = allocator;

    RgDevice *device = PlatformGetDevice(uniform_buffer->platform);
    RgLimits limits;
    rgDeviceGetLimits(device, &limits);

    uniform_buffer->size = 65536;
    uniform_buffer->alignment = limits.min_uniform_buffer_offset_alignment;

    RgBufferInfo buffer_info = {};
    buffer_info.size = uniform_buffer->size;
    buffer_info.usage = RG_BUFFER_USAGE_UNIFORM | RG_BUFFER_USAGE_TRANSFER_DST;
    buffer_info.memory = RG_BUFFER_MEMORY_HOST;
    uniform_buffer->buffer = rgBufferCreate(device, &buffer_info);

    uniform_buffer->mapping = (uint8_t*)rgBufferMap(device, uniform_buffer->buffer);

    return uniform_buffer;
}

void UniformBufferReset(UniformBuffer *uniform_buffer)
{
    uniform_buffer->offset = 0;
}

uint32_t UniformBufferUse(UniformBuffer *uniform_buffer, void *data, size_t size)
{
    uniform_buffer->offset = alignTo(uniform_buffer->offset, uniform_buffer->alignment);
    size_t old_offset = uniform_buffer->offset;
    uniform_buffer->offset += size;
    assert(uniform_buffer->offset <= uniform_buffer->size);
    memcpy(uniform_buffer->mapping + old_offset, data, size);
    return (uint32_t)old_offset;
}

void UniformBufferGetDescriptor(UniformBuffer *uniform_buffer, RgDescriptor *descriptor)
{
    descriptor->buffer.buffer = uniform_buffer->buffer;
    descriptor->buffer.offset = 0;
    descriptor->buffer.range = 0;
}

void UniformBufferDestroy(UniformBuffer *uniform_buffer)
{
    RgDevice *device = PlatformGetDevice(uniform_buffer->platform);
    rgBufferUnmap(device, uniform_buffer->buffer);
    rgBufferDestroy(device, uniform_buffer->buffer);
    Free(uniform_buffer->allocator, uniform_buffer);
}

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

    RgPipeline *offscreen_pipeline;
    RgPipeline *backbuffer_pipeline;

    RgDescriptorSet *camera_set;

    RgDescriptorSet *descriptor_set;

    UniformBuffer *uniform_buffer;
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

    app->camera_set = rgPipelineCreateDescriptorSet(app->offscreen_pipeline, 0);

    app->cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);
    app->cmd_buffers[0] = rgCmdBufferCreate(device, app->cmd_pool);
    app->cmd_buffers[1] = rgCmdBufferCreate(device, app->cmd_pool);

    AppResize(app);

    FPSCameraInit(&app->camera, app->platform);
    
    app->uniform_buffer = UniformBufferCreate(app->platform, NULL);
    RgDescriptor uniform_descriptor;
    UniformBufferGetDescriptor(app->uniform_buffer, &uniform_descriptor);
    rgUpdateDescriptorSet(app->camera_set, 1, &uniform_descriptor);

    app->cube_mesh = MeshCreateCube(app->platform, app->cmd_pool, NULL);

    app->last_time = PlatformGetTime(app->platform);

    return app;
}

void AppDestroy(App *app)
{
    RgDevice *device = PlatformGetDevice(app->platform);

    UniformBufferDestroy(app->uniform_buffer);
    MeshDestroy(app->cube_mesh);

    rgPipelineDestroyDescriptorSet(app->backbuffer_pipeline, app->descriptor_set);
    rgPipelineDestroyDescriptorSet(app->offscreen_pipeline, app->camera_set);

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
    offscreen_clear_values[1].depth_stencil = { 1.0f, 0 };
    rgCmdSetRenderPass(cmd_buffer, offscreen_pass, 2, offscreen_clear_values);

    rgCmdBindPipeline(cmd_buffer, app->offscreen_pipeline);
    uint32_t uniform_offset =
        UniformBufferUse(app->uniform_buffer, &camera_uniform, sizeof(camera_uniform));
    rgCmdBindDescriptorSet(cmd_buffer, app->camera_set, 1, &uniform_offset);

    rgCmdBindVertexBuffer(
            cmd_buffer, MeshGetVertexBuffer(app->cube_mesh), 0);
    rgCmdBindIndexBuffer(
            cmd_buffer, MeshGetIndexBuffer(app->cube_mesh), 0, RG_INDEX_TYPE_UINT32);

    rgCmdDrawIndexed(cmd_buffer, MeshGetIndexCount(app->cube_mesh), 1, 0, 0, 0);

    // Backbuffer pass

    RgClearValue backbuffer_clear_values[1];
    backbuffer_clear_values[0].color = { { 0.0, 0.0, 0.0, 1.0 } };
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
    if (app->current_frame == 0)
    {
        UniformBufferReset(app->uniform_buffer);
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

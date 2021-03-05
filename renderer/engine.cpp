#include "engine.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <rg.h>
#include "allocator.h"
#include "lexer.h"
#include "string_map.hpp"
#include "config.h"
#include "pipeline_util.h"
#include "pbr.h"
#include "pool.h"
#include <tinyshader/tinyshader.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#include <unistd.h>
#include <linux/limits.h>
#endif

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define EVENT_CAPACITY 1024

static struct
{
    Event events[EVENT_CAPACITY];
    size_t head;
    size_t tail;
} event_queue = {{}, 0, 0};

static void eventQueueWindowPosCallback(GLFWwindow *window, int x, int y);
static void eventQueueWindowSizeCallback(GLFWwindow *window, int width, int height);
static void eventQueueWindowCloseCallback(GLFWwindow *window);
static void eventQueueWindowRefreshCallback(GLFWwindow *window);
static void eventQueueWindowFocusCallback(GLFWwindow *window, int focused);
static void eventQueueWindowIconifyCallback(GLFWwindow *window, int iconified);
static void eventQueueFramebufferSizeCallback(GLFWwindow *window, int width, int height);
static void
eventQueueMouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
static void eventQueueCursorPosCallback(GLFWwindow *window, double x, double y);
static void eventQueueCursorEnterCallback(GLFWwindow *window, int entered);
static void eventQueueScrollCallback(GLFWwindow *window, double x, double y);
static void
eventQueueKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void eventQueueCharCallback(GLFWwindow *window, unsigned int codepoint);
static void eventQueueMonitorCallback(GLFWmonitor *monitor, int action);
static void eventQueueFileDropCallback(GLFWwindow *window, int count, const char **paths);
static void eventQueueJoystickCallback(int jid, int action);
static void eventQueueWindowMaximizeCallback(GLFWwindow *window, int maximized);
static void
eventQueueWindowContentScaleCallback(GLFWwindow *window, float xscale, float yscale);
static void eventFree(Event *event);

static const char *getExeDirPath(Allocator *allocator)
{
#if defined(__linux__)
    char buf[PATH_MAX];
    size_t buf_size = readlink("/proc/self/exe", buf, sizeof(buf));

    char *path = (char *)Allocate(allocator, buf_size + 1);
    memcpy(path, buf, buf_size);
    path[buf_size] = '\0';

    size_t last_slash_pos = 0;
    for (size_t i = 0; i < buf_size; ++i)
    {
        if (path[i] == '/')
        {
            last_slash_pos = i;
        }
    }

    path[last_slash_pos] = '\0';

    return path;
#elif defined(_WIN32)
    char tmp_buf[MAX_PATH];
    DWORD buf_size = GetModuleFileNameA(NULL, tmp_buf, sizeof(tmp_buf));
    char *path = (char *)Allocate(allocator, buf_size + 1);
    GetModuleFileNameA(GetModuleHandle(NULL), path, buf_size + 1);

    size_t last_slash_pos = 0;
    for (size_t i = 0; i < buf_size; ++i)
    {
        if (path[i] == '\\')
        {
            last_slash_pos = i;
        }
    }

    path[last_slash_pos] = '\0';

    return path;
#else
#error unsupported system
#endif
}

struct Engine
{
    Allocator *allocator;
    Arena *arena;

    GLFWwindow *window;
    RgDevice *device;
    RgSwapchain *swapchain;

    const char *exe_dir;

    RgCmdPool *graphics_cmd_pool;
    RgCmdPool *transfer_cmd_pool;
    ImageHandle white_image;
    ImageHandle black_image;
    SamplerHandle default_sampler;

    ImageHandle brdf_image;

    RgDescriptorSetLayout *global_set_layout;
    RgPipelineLayout *global_pipeline_layout;
    RgDescriptorSet *global_descriptor_set;

    Pool *storage_buffer_pool;
    Pool *texture_pool;
    Pool *sampler_pool;
};

static void EngineResizeResources(Engine *engine)
{
    int width, height;
    glfwGetFramebufferSize(engine->window, &width, &height);

    RgSwapchain *old_swapchain = engine->swapchain;

    RgSwapchainInfo swapchain_info = {};
    swapchain_info.vsync = false;
    swapchain_info.old_swapchain = old_swapchain;

    swapchain_info.width = width;
    swapchain_info.height = height;

#ifdef _WIN32
    swapchain_info.window_handle = (void *)glfwGetWin32Window(engine->window),
#else
    swapchain_info.window_handle = (void *)glfwGetX11Window(engine->window),
    swapchain_info.display_handle = (void *)glfwGetX11Display(),
#endif

    engine->swapchain = rgSwapchainCreate(engine->device, &swapchain_info);

    if (old_swapchain)
    {
        rgSwapchainDestroy(engine->device, old_swapchain);
    }
}

extern "C" Engine *EngineCreate(Allocator *allocator)
{
    Engine *engine = (Engine *)Allocate(allocator, sizeof(Engine));
    *engine = {};

    engine->allocator = allocator;
    engine->arena = ArenaCreate(engine->allocator, 4194304); // 4MiB

    engine->exe_dir = getExeDirPath(allocator);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan renderer", NULL, NULL);
    glfwSetWindowUserPointer(window, engine);

    glfwSetMonitorCallback(eventQueueMonitorCallback);
    glfwSetJoystickCallback(eventQueueJoystickCallback);
    glfwSetWindowPosCallback(window, eventQueueWindowPosCallback);
    glfwSetWindowSizeCallback(window, eventQueueWindowSizeCallback);
    glfwSetWindowCloseCallback(window, eventQueueWindowCloseCallback);
    glfwSetWindowRefreshCallback(window, eventQueueWindowRefreshCallback);
    glfwSetWindowFocusCallback(window, eventQueueWindowFocusCallback);
    glfwSetWindowIconifyCallback(window, eventQueueWindowIconifyCallback);
    glfwSetFramebufferSizeCallback(window, eventQueueFramebufferSizeCallback);
    glfwSetMouseButtonCallback(window, eventQueueMouseButtonCallback);
    glfwSetCursorPosCallback(window, eventQueueCursorPosCallback);
    glfwSetCursorEnterCallback(window, eventQueueCursorEnterCallback);
    glfwSetScrollCallback(window, eventQueueScrollCallback);
    glfwSetKeyCallback(window, eventQueueKeyCallback);
    glfwSetCharCallback(window, eventQueueCharCallback);
    glfwSetDropCallback(window, eventQueueFileDropCallback);
    glfwSetWindowMaximizeCallback(window, eventQueueWindowMaximizeCallback);
    glfwSetWindowContentScaleCallback(window, eventQueueWindowContentScaleCallback);

    engine->window = window;

    RgDeviceInfo device_info = {};
    device_info.enable_validation = true;
    engine->device = rgDeviceCreate(&device_info);

    EngineResizeResources(engine);

    RgDevice *device = engine->device;

    engine->storage_buffer_pool = PoolCreate(engine->allocator, 4 * 1024);
    engine->texture_pool = PoolCreate(engine->allocator, 4 * 1024);
    engine->sampler_pool = PoolCreate(engine->allocator, 4 * 1024);

    {
        RgDescriptorSetLayoutEntry entries[] = {
            {
                0,                                             // binding
                RG_DESCRIPTOR_STORAGE_BUFFER,                  // type
                RG_SHADER_STAGE_ALL,                           // shader_stages
                PoolGetSlotCount(engine->storage_buffer_pool), // count
            },
            {
                1,                                      // binding
                RG_DESCRIPTOR_IMAGE,                    // type
                RG_SHADER_STAGE_ALL,                    // shader_stages
                PoolGetSlotCount(engine->texture_pool), // count
            },
            {
                2,                                      // binding
                RG_DESCRIPTOR_SAMPLER,                  // type
                RG_SHADER_STAGE_ALL,                    // shader_stages
                PoolGetSlotCount(engine->sampler_pool), // count
            },
        };

        RgDescriptorSetLayoutInfo info = {};
        info.entries = entries;
        info.entry_count = sizeof(entries) / sizeof(entries[0]);

        engine->global_set_layout = rgDescriptorSetLayoutCreate(device, &info);

        RgPipelineLayoutInfo pipeline_layout_info = {};
        pipeline_layout_info.set_layouts = &engine->global_set_layout;
        pipeline_layout_info.set_layout_count = 1;
        engine->global_pipeline_layout =
            rgPipelineLayoutCreate(device, &pipeline_layout_info);

        engine->global_descriptor_set =
            rgDescriptorSetCreate(device, engine->global_set_layout);
    }

    engine->transfer_cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_TRANSFER);
    engine->graphics_cmd_pool = rgCmdPoolCreate(device, RG_QUEUE_TYPE_GRAPHICS);

    RgImageInfo image_info = {};
    image_info.extent = {1, 1, 1};
    image_info.format = RG_FORMAT_RGBA8_UNORM;
    image_info.usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_TRANSFER_DST;
    image_info.aspect = RG_IMAGE_ASPECT_COLOR;
    image_info.sample_count = 1;
    image_info.mip_count = 1;
    image_info.layer_count = 1;

    engine->white_image = EngineAllocateImageHandle(engine, &image_info);
    engine->black_image = EngineAllocateImageHandle(engine, &image_info);

    uint8_t white_data[] = {255, 255, 255, 255};
    uint8_t black_data[] = {0, 0, 0, 255};

    RgImageCopy image_copy = {};
    RgExtent3D extent = {1, 1, 1};

    image_copy.image = engine->white_image.image;
    rgImageUpload(
        device,
        engine->transfer_cmd_pool,
        &image_copy,
        &extent,
        sizeof(white_data),
        white_data);

    image_copy.image = engine->black_image.image;
    rgImageUpload(
        device,
        engine->transfer_cmd_pool,
        &image_copy,
        &extent,
        sizeof(black_data),
        black_data);

    RgSamplerInfo sampler_info = {};
    sampler_info.anisotropy = true;
    sampler_info.max_anisotropy = 16.0f;
    sampler_info.min_filter = RG_FILTER_LINEAR;
    sampler_info.mag_filter = RG_FILTER_LINEAR;
    sampler_info.address_mode = RG_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.border_color = RG_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    engine->default_sampler = EngineAllocateSamplerHandle(engine, &sampler_info);

    engine->brdf_image = GenerateBRDFLUT(engine, engine->graphics_cmd_pool, 512);

    return engine;
}

extern "C" void EngineDestroy(Engine *engine)
{
    RgDevice *device = engine->device;

    rgPipelineLayoutDestroy(device, engine->global_pipeline_layout);
    rgDescriptorSetDestroy(device, engine->global_descriptor_set);
    rgDescriptorSetLayoutDestroy(device, engine->global_set_layout);

    EngineFreeImageHandle(engine, &engine->brdf_image);
    EngineFreeImageHandle(engine, &engine->white_image);
    EngineFreeImageHandle(engine, &engine->black_image);
    EngineFreeSamplerHandle(engine, &engine->default_sampler);
    rgCmdPoolDestroy(device, engine->transfer_cmd_pool);
    rgCmdPoolDestroy(device, engine->graphics_cmd_pool);

    PoolDestroy(engine->storage_buffer_pool);
    PoolDestroy(engine->texture_pool);
    PoolDestroy(engine->sampler_pool);

    rgSwapchainDestroy(engine->device, engine->swapchain);
    rgDeviceDestroy(engine->device);

    glfwDestroyWindow(engine->window);
    glfwTerminate();

    ArenaDestroy(engine->arena);

    Free(engine->allocator, (void *)engine->exe_dir);
    Free(engine->allocator, engine);
}

extern "C" RgDevice *EngineGetDevice(Engine *engine)
{
    return engine->device;
}

extern "C" RgSwapchain *EngineGetSwapchain(Engine *engine)
{
    return engine->swapchain;
}

extern "C" double EngineGetTime(Engine *engine)
{
    (void)engine;
    return glfwGetTime();
}

extern "C" void EngineGetWindowSize(Engine *engine, uint32_t *width, uint32_t *height)
{
    int iwidth, iheight;
    glfwGetFramebufferSize(engine->window, &iwidth, &iheight);
    *width = (uint32_t)iwidth;
    *height = (uint32_t)iheight;
}

extern "C" bool EngineGetCursorEnabled(Engine *engine)
{
    return glfwGetInputMode(engine->window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL;
}

extern "C" void EngineSetCursorEnabled(Engine *engine, bool enabled)
{
    int mode = enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
    glfwSetInputMode(engine->window, GLFW_CURSOR, mode);
}

extern "C" void EngineGetCursorPos(Engine *engine, double *x, double *y)
{
    glfwGetCursorPos(engine->window, x, y);
}

extern "C" bool EngineGetKeyState(Engine *engine, Key key)
{
    int state = glfwGetKey(engine->window, key);
    switch (state)
    {
    case GLFW_PRESS:
    case GLFW_REPEAT: return true;
    default: return false;
    }
    return false;
}

extern "C" bool EngineGetButtonState(Engine *engine, Button button)
{
    int state = glfwGetMouseButton(engine->window, button);
    switch (state)
    {
    case GLFW_PRESS:
    case GLFW_REPEAT: return true;
    default: return false;
    }
    return false;
}

extern "C" bool EngineShouldClose(Engine *engine)
{
    return glfwWindowShouldClose(engine->window);
}

extern "C" void EnginePollEvents(Engine *engine)
{
    (void)engine;
    glfwPollEvents();
}

extern "C" bool EngineNextEvent(Engine *engine, Event *event)
{
    memset(event, 0, sizeof(Event));

    if (event_queue.head != event_queue.tail)
    {
        *event = event_queue.events[event_queue.tail];
        event_queue.tail = (event_queue.tail + 1) % EVENT_CAPACITY;
    }

    if (event->type == EVENT_WINDOW_RESIZED)
    {
        EngineResizeResources(engine);
    }

    return event->type != EVENT_NONE;
}

extern "C" const char *EngineGetExeDir(Engine *engine)
{
    return engine->exe_dir;
}

extern "C" uint8_t *EngineLoadFileRelative(
    Engine *engine, Allocator *allocator, const char *relative_path, size_t *size)
{
    size_t path_size = strlen(engine->exe_dir) + 1 + strlen(relative_path) + 1;
    char *path = (char *)Allocate(allocator, path_size);
    snprintf(path, path_size, "%s/%s", engine->exe_dir, relative_path);
    path[path_size - 1] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = (uint8_t *)Allocate(allocator, *size);
    size_t read_size = fread(data, 1, *size, f);
    EG_ASSERT(read_size == *size);

    fclose(f);

    Free(allocator, path);

    return data;
}

RgCmdPool *EngineGetTransferCmdPool(Engine *engine)
{
    return engine->transfer_cmd_pool;
}

extern "C" ImageHandle EngineGetWhiteImage(Engine *engine)
{
    return engine->white_image;
}

extern "C" ImageHandle EngineGetBlackImage(Engine *engine)
{
    return engine->black_image;
}

extern "C" ImageHandle EngineGetBRDFImage(Engine *engine)
{
    return engine->brdf_image;
}

extern "C" SamplerHandle EngineGetDefaultSampler(Engine *engine)
{
    return engine->default_sampler;
}

extern "C" RgPipeline *EngineCreateGraphicsPipeline(Engine *engine, const char *path)
{
    RgPipelineLayout *pipeline_layout = engine->global_pipeline_layout;
    EG_ASSERT(pipeline_layout);

    size_t hlsl_size = 0;
    char *hlsl =
        (char *)EngineLoadFileRelative(engine, engine->allocator, path, &hlsl_size);
    EG_ASSERT(hlsl);

    RgPipeline *pipeline = PipelineUtilCreateGraphicsPipeline(
        engine, engine->allocator, pipeline_layout, hlsl, hlsl_size);
    EG_ASSERT(pipeline);

    Free(engine->allocator, hlsl);

    return pipeline;
}

extern "C" RgPipeline *EngineCreateComputePipeline(Engine *engine, const char *path)
{
    RgDevice *device = EngineGetDevice(engine);

    RgPipelineLayout *pipeline_layout = engine->global_pipeline_layout;
    EG_ASSERT(pipeline_layout);

    size_t hlsl_size = 0;
    char *hlsl =
        (char *)EngineLoadFileRelative(engine, engine->allocator, path, &hlsl_size);
    EG_ASSERT(hlsl);

    uint8_t *spv_code = NULL;
    size_t spv_code_size = 0;

    {
        TsCompilerOptions *options = tsCompilerOptionsCreate();
        tsCompilerOptionsSetStage(options, TS_SHADER_STAGE_VERTEX);
        const char *entry_point = "main";
        tsCompilerOptionsSetEntryPoint(options, entry_point, strlen(entry_point));
        tsCompilerOptionsSetSource(options, hlsl, hlsl_size, NULL, 0);

        TsCompilerOutput *output = tsCompile(options);
        const char *errors = tsCompilerOutputGetErrors(output);
        if (errors)
        {
            fprintf(stderr, "Shader compilation error:\n%s\n", errors);

            tsCompilerOutputDestroy(output);
            tsCompilerOptionsDestroy(options);
            exit(1);
        }

        size_t spirv_size = 0;
        const uint8_t *spirv = tsCompilerOutputGetSpirv(output, &spirv_size);

        spv_code = (uint8_t *)Allocate(engine->allocator, spirv_size);
        memcpy(spv_code, spirv, spirv_size);
        spv_code_size = spirv_size;

        tsCompilerOutputDestroy(output);
        tsCompilerOptionsDestroy(options);
    }

    RgComputePipelineInfo info = {};
    info.pipeline_layout = pipeline_layout;
    info.code = spv_code;
    info.code_size = spv_code_size;
    RgPipeline *pipeline = rgComputePipelineCreate(device, &info);
    EG_ASSERT(pipeline);

    Free(engine->allocator, spv_code);
    Free(engine->allocator, hlsl);

    return pipeline;
}

RgPipelineLayout *EngineGetGlobalPipelineLayout(Engine *engine)
{
    return engine->global_pipeline_layout;
}

RgDescriptorSet *EngineGetGlobalDescriptorSet(Engine *engine)
{
    return engine->global_descriptor_set;
}

static uint32_t EngineAllocateDescriptor(
    Engine *engine, Pool *pool, uint32_t binding, const RgDescriptor *descriptor)
{
    EG_ASSERT(engine->global_descriptor_set);

    uint32_t handle = PoolAllocateSlot(pool);
    if (handle == UINT32_MAX) return handle;

    RgDevice *device = EngineGetDevice(engine);

    RgDescriptorUpdateInfo entry = {};
    entry.binding = binding;
    entry.base_index = handle;
    entry.descriptor_count = 1;
    entry.descriptors = descriptor;

    rgDescriptorSetUpdate(device, engine->global_descriptor_set, &entry, 1);

    return handle;
}

static void EngineFreeDescriptor(Engine *engine, Pool *pool, uint32_t handle)
{
    (void)engine;
    PoolFreeSlot(pool, handle);
}

extern "C" BufferHandle
EngineAllocateStorageBufferHandle(Engine *engine, RgBufferInfo *info)
{
    BufferHandle handle = {};
    handle.buffer = rgBufferCreate(engine->device, info);

    RgDescriptor descriptor = {};
    descriptor.buffer.buffer = handle.buffer;
    descriptor.buffer.offset = 0;
    descriptor.buffer.size = 0;

    handle.index =
        EngineAllocateDescriptor(engine, engine->storage_buffer_pool, 0, &descriptor);

    EG_ASSERT(handle.index != UINT32_MAX);

    return handle;
}

extern "C" void EngineFreeStorageBufferHandle(Engine *engine, BufferHandle *handle)
{
    EngineFreeDescriptor(engine, engine->storage_buffer_pool, handle->index);
    rgBufferDestroy(engine->device, handle->buffer);
}

extern "C" ImageHandle EngineAllocateImageHandle(Engine *engine, RgImageInfo *info)
{
    ImageHandle handle = {};
    handle.image = rgImageCreate(engine->device, info);

    RgDescriptor descriptor = {};
    descriptor.image.image = handle.image;

    handle.index = EngineAllocateDescriptor(engine, engine->texture_pool, 1, &descriptor);

    EG_ASSERT(handle.index != UINT32_MAX);

    return handle;
}

extern "C" void EngineFreeImageHandle(Engine *engine, ImageHandle *handle)
{
    EngineFreeDescriptor(engine, engine->texture_pool, handle->index);
    rgImageDestroy(engine->device, handle->image);
}

extern "C" SamplerHandle EngineAllocateSamplerHandle(Engine *engine, RgSamplerInfo *info)
{
    SamplerHandle handle = {};
    handle.sampler = rgSamplerCreate(engine->device, info);

    RgDescriptor descriptor = {};
    descriptor.image.sampler = handle.sampler;

    handle.index = EngineAllocateDescriptor(engine, engine->sampler_pool, 2, &descriptor);

    EG_ASSERT(handle.index != UINT32_MAX);

    return handle;
}

extern "C" void EngineFreeSamplerHandle(Engine *engine, SamplerHandle *handle)
{
    EngineFreeDescriptor(engine, engine->sampler_pool, handle->index);
    rgSamplerDestroy(engine->device, handle->sampler);
}

// Event queue {{{
static char *eventQueueStrdup(const char *string)
{
    const size_t size = strlen(string) + 1;
    char *result = (char *)malloc(size);
    memcpy(result, string, size);
    return result;
}

static Event *eventQueueNewEvent(void)
{
    Event *event = event_queue.events + event_queue.head;
    event_queue.head = (event_queue.head + 1) % EVENT_CAPACITY;
    EG_ASSERT(event_queue.head != event_queue.tail);
    memset(event, 0, sizeof(Event));
    return event;
}

static void eventQueueWindowPosCallback(GLFWwindow *window, int x, int y)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_MOVED;
    event->window = window;
    event->pos.x = x;
    event->pos.y = y;
}

static void eventQueueWindowSizeCallback(GLFWwindow *window, int width, int height)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_RESIZED;
    event->window = window;
    event->size.width = width;
    event->size.height = height;
}

static void eventQueueWindowCloseCallback(GLFWwindow *window)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_CLOSED;
    event->window = window;
}

static void eventQueueWindowRefreshCallback(GLFWwindow *window)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_REFRESH;
    event->window = window;
}

static void eventQueueWindowFocusCallback(GLFWwindow *window, int focused)
{
    Event *event = eventQueueNewEvent();
    event->window = window;

    if (focused)
        event->type = EVENT_WINDOW_FOCUSED;
    else
        event->type = EVENT_WINDOW_DEFOCUSED;
}

static void eventQueueWindowIconifyCallback(GLFWwindow *window, int iconified)
{
    Event *event = eventQueueNewEvent();
    event->window = window;

    if (iconified)
        event->type = EVENT_WINDOW_ICONIFIED;
    else
        event->type = EVENT_WINDOW_UNICONIFIED;
}

static void eventQueueFramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_FRAMEBUFFER_RESIZED;
    event->window = window;
    event->size.width = width;
    event->size.height = height;
}

static void
eventQueueMouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    Event *event = eventQueueNewEvent();
    event->window = window;
    event->mouse.button = button;
    event->mouse.mods = mods;

    if (action == GLFW_PRESS)
        event->type = EVENT_BUTTON_PRESSED;
    else if (action == GLFW_RELEASE)
        event->type = EVENT_BUTTON_RELEASED;
}

static void eventQueueCursorPosCallback(GLFWwindow *window, double x, double y)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_CURSOR_MOVED;
    event->window = window;
    event->pos.x = (int)x;
    event->pos.y = (int)y;
}

static void eventQueueCursorEnterCallback(GLFWwindow *window, int entered)
{
    Event *event = eventQueueNewEvent();
    event->window = window;

    if (entered)
        event->type = EVENT_CURSOR_ENTERED;
    else
        event->type = EVENT_CURSOR_LEFT;
}

static void eventQueueScrollCallback(GLFWwindow *window, double x, double y)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_SCROLLED;
    event->window = window;
    event->scroll.x = x;
    event->scroll.y = y;
}

static void
eventQueueKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    Event *event = eventQueueNewEvent();
    event->window = window;
    event->keyboard.key = key;
    event->keyboard.scancode = scancode;
    event->keyboard.mods = mods;

    if (action == GLFW_PRESS)
        event->type = EVENT_KEY_PRESSED;
    else if (action == GLFW_RELEASE)
        event->type = EVENT_KEY_RELEASED;
    else if (action == GLFW_REPEAT)
        event->type = EVENT_KEY_REPEATED;
}

static void eventQueueCharCallback(GLFWwindow *window, unsigned int codepoint)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_CODEPOINT_INPUT;
    event->window = window;
    event->codepoint = codepoint;
}

static void eventQueueMonitorCallback(GLFWmonitor *monitor, int action)
{
    Event *event = eventQueueNewEvent();
    event->monitor = monitor;

    if (action == GLFW_CONNECTED)
        event->type = EVENT_MONITOR_CONNECTED;
    else if (action == GLFW_DISCONNECTED)
        event->type = EVENT_MONITOR_DISCONNECTED;
}

static void eventQueueFileDropCallback(GLFWwindow *window, int count, const char **paths)
{
    Event *event = eventQueueNewEvent();
    event->type = EVENT_FILE_DROPPED;
    event->window = window;
    event->file.paths = (char **)malloc(count * sizeof(char *));
    event->file.count = count;

    while (count--)
        event->file.paths[count] = eventQueueStrdup(paths[count]);
}

static void eventQueueJoystickCallback(int jid, int action)
{
    Event *event = eventQueueNewEvent();
    event->joystick = jid;

    if (action == GLFW_CONNECTED)
        event->type = EVENT_JOYSTICK_CONNECTED;
    else if (action == GLFW_DISCONNECTED)
        event->type = EVENT_JOYSTICK_DISCONNECTED;
}

static void eventQueueWindowMaximizeCallback(GLFWwindow *window, int maximized)
{
    Event *event = eventQueueNewEvent();
    event->window = window;

    if (maximized)
        event->type = EVENT_WINDOW_MAXIMIZED;
    else
        event->type = EVENT_WINDOW_UNMAXIMIZED;
}

static void
eventQueueWindowContentScaleCallback(GLFWwindow *window, float xscale, float yscale)
{
    Event *event = eventQueueNewEvent();
    event->window = window;
    event->type = EVENT_WINDOW_SCALE_CHANGED;
    event->scale.x = xscale;
    event->scale.y = yscale;
}

static inline void eventFree(Event *event)
{
    if (event->type == EVENT_FILE_DROPPED)
    {
        while (event->file.count--)
            free(event->file.paths[event->file.count]);

        free(event->file.paths);
    }

    memset(event, 0, sizeof(Event));
}
// }}}

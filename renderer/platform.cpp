#include "platform.h"

#include <rg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#ifdef __linux__
#include <unistd.h>
#include <linux/limits.h>
#endif

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define EVENT_CAPACITY 1024

static struct
{
    Event events[EVENT_CAPACITY];
    size_t head;
    size_t tail;
} event_queue = { {}, 0, 0 };

static void eventQueueWindowPosCallback(GLFWwindow* window, int x, int y);
static void eventQueueWindowSizeCallback(GLFWwindow* window, int width, int height);
static void eventQueueWindowCloseCallback(GLFWwindow* window);
static void eventQueueWindowRefreshCallback(GLFWwindow* window);
static void eventQueueWindowFocusCallback(GLFWwindow* window, int focused);
static void eventQueueWindowIconifyCallback(GLFWwindow* window, int iconified);
static void eventQueueFramebufferSizeCallback(GLFWwindow* window, int width, int height);
static void eventQueueMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void eventQueueCursorPosCallback(GLFWwindow* window, double x, double y);
static void eventQueueCursorEnterCallback(GLFWwindow* window, int entered);
static void eventQueueScrollCallback(GLFWwindow* window, double x, double y);
static void eventQueueKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void eventQueueCharCallback(GLFWwindow* window, unsigned int codepoint);
static void eventQueueMonitorCallback(GLFWmonitor* monitor, int action);
static void eventQueueFileDropCallback(GLFWwindow* window, int count, const char** paths);
static void eventQueueJoystickCallback(int jid, int action);
static void eventQueueWindowMaximizeCallback(GLFWwindow* window, int maximized);
static void eventQueueWindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale);
static void eventFree(Event* event);

static const char *getExeDirPath()
{
#ifdef __linux__
    char buf[PATH_MAX];
    size_t buf_size = readlink("/proc/self/exe", buf, sizeof(buf));

    char *path = new char[buf_size+1];
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
#endif
}

struct Platform
{
    GLFWwindow *window;
    RgDevice *device;
    RgSwapchain *swapchain;
    const char *exe_dir;

    RgCmdPool *transfer_cmd_pool;
    RgImage *white_image;
    RgImage *black_image;
    RgSampler *default_sampler;
};

void PlatformResizeResources(Platform *platform)
{
    int width, height;
    glfwGetFramebufferSize(platform->window, &width, &height);

    RgSwapchain *old_swapchain = platform->swapchain;

    RgSwapchainInfo swapchain_info = {};
    swapchain_info.vsync = true;
    swapchain_info.old_swapchain = old_swapchain;

    swapchain_info.width = width;
    swapchain_info.height = height;

#ifdef _WIN32
    swapchain_info.window_handle = (void*)glfwGetWin32Window(platform->window),
#else
    swapchain_info.window_handle = (void*)glfwGetX11Window(platform->window),
    swapchain_info.display_handle = (void*)glfwGetX11Display(),
#endif

    platform->swapchain = rgSwapchainCreate(platform->device, &swapchain_info);

    if (old_swapchain)
    {
        rgSwapchainDestroy(platform->device, old_swapchain);
    }
}

Platform *PlatformCreate(const char *window_title)
{
    Platform *platform = new Platform();
    platform->exe_dir = getExeDirPath();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, window_title, NULL, NULL);
    glfwSetWindowUserPointer(window, platform);

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

    platform->window = window;

    RgDeviceInfo device_info = {};
    device_info.enable_validation = true;
    platform->device = rgDeviceCreate(&device_info);

    PlatformResizeResources(platform);

    platform->transfer_cmd_pool = rgCmdPoolCreate(platform->device, RG_QUEUE_TYPE_TRANSFER);

    RgImageInfo image_info = {};
    image_info.extent = {1, 1, 1};
    image_info.format = RG_FORMAT_RGBA8_UNORM;
    image_info.usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_TRANSFER_DST;
    image_info.aspect = RG_IMAGE_ASPECT_COLOR;
    image_info.sample_count = 1;
    image_info.mip_count = 1;
    image_info.layer_count = 1;

    platform->white_image = rgImageCreate(platform->device, &image_info);
    platform->black_image = rgImageCreate(platform->device, &image_info);

    uint8_t white_data[] = {0, 0, 0, 255};
    uint8_t black_data[] = {0, 0, 0, 255};

    RgImageCopy image_copy = {};
    RgExtent3D extent = {1, 1, 1};

    image_copy.image = platform->white_image;
    rgImageUpload(
            platform->device,
            platform->transfer_cmd_pool,
            &image_copy,
            &extent,
            sizeof(white_data),
            white_data);

    image_copy.image = platform->black_image;
    rgImageUpload(
            platform->device,
            platform->transfer_cmd_pool,
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
    platform->default_sampler = rgSamplerCreate(platform->device, &sampler_info);

    return platform;
}

void PlatformDestroy(Platform *platform)
{
    rgImageDestroy(platform->device, platform->white_image);
    rgImageDestroy(platform->device, platform->black_image);
    rgSamplerDestroy(platform->device, platform->default_sampler);
    rgCmdPoolDestroy(platform->device, platform->transfer_cmd_pool);
    rgSwapchainDestroy(platform->device, platform->swapchain);
    rgDeviceDestroy(platform->device);

    glfwDestroyWindow(platform->window);
    glfwTerminate();

    delete[] platform->exe_dir;

    delete platform;
}

const char *PlatformGetExeDir(Platform *platform)
{
    return platform->exe_dir;
}

RgDevice *PlatformGetDevice(Platform *platform)
{
    return platform->device;
}

RgSwapchain *PlatformGetSwapchain(Platform *platform)
{
    return platform->swapchain;
}

double PlatformGetTime(Platform *platform)
{
    (void)platform;
    return glfwGetTime();
}

void PlatformGetWindowSize(Platform *platform, uint32_t *width, uint32_t *height)
{
    int iwidth, iheight;
    glfwGetFramebufferSize(platform->window, &iwidth, &iheight);
    *width = (uint32_t)iwidth;
    *height = (uint32_t)iheight;
}

bool PlatformGetCursorEnabled(Platform *platform)
{
    return glfwGetInputMode(platform->window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL;
}

void PlatformSetCursorEnabled(Platform *platform, bool enabled)
{
    int mode = enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
    glfwSetInputMode(platform->window, GLFW_CURSOR, mode);
}

void PlatformGetCursorPos(Platform *platform, double *x, double *y)
{
    glfwGetCursorPos(platform->window, x, y);
}

bool PlatformGetKeyState(Platform *platform, Key key)
{
    int state = glfwGetKey(platform->window, key);
    switch (state)
    {
    case GLFW_PRESS:
    case GLFW_REPEAT: return true;
    default: return false;
    }
    return false;
}

bool PlatformGetButtonState(Platform *platform, Button button)
{
    int state = glfwGetMouseButton(platform->window, button);
    switch (state)
    {
    case GLFW_PRESS:
    case GLFW_REPEAT: return true;
    default: return false;
    }
    return false;
}

bool PlatformShouldClose(Platform *platform)
{
    return glfwWindowShouldClose(platform->window);
}

void PlatformPollEvents(Platform *platform)
{
    (void)platform;
    glfwPollEvents();
}

bool PlatformNextEvent(Platform *platform, Event *event)
{
    memset(event, 0, sizeof(Event));

    if (event_queue.head != event_queue.tail)
    {
        *event = event_queue.events[event_queue.tail];
        event_queue.tail = (event_queue.tail + 1) % EVENT_CAPACITY;
    }

    if (event->type == EVENT_WINDOW_RESIZED)
    {
        PlatformResizeResources(platform);
    }

    return event->type != EVENT_NONE;
}

uint8_t *PlatformLoadFileRelative(Platform *platform, const char *relative_path, size_t *size)
{
    size_t path_size = strlen(platform->exe_dir) + 1 + strlen(relative_path) + 1;
    char *path = new char[path_size];
    snprintf(path, path_size, "%s/%s", platform->exe_dir, relative_path);
    path[path_size-1] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = new uint8_t[*size];
    fread(data, *size, 1, f);

    fclose(f);

    delete[] path;

    return data;
}

RgImage *PlatformGetWhiteImage(Platform *platform)
{
    return platform->white_image;
}

RgImage *PlatformGetBlackImage(Platform *platform)
{
    return platform->black_image;
}

RgSampler *PlatformGetDefaultSampler(Platform *platform)
{
    return platform->default_sampler;
}

// Event queue {{{
static char* eventQueueStrdup(const char* string)
{
    const size_t size = strlen(string) + 1;
    char* result = (char*) malloc(size);
    memcpy(result, string, size);
    return result;
}

static Event* eventQueueNewEvent(void)
{
    Event* event = event_queue.events + event_queue.head;
    event_queue.head = (event_queue.head + 1) % EVENT_CAPACITY;
    assert(event_queue.head != event_queue.tail);
    memset(event, 0, sizeof(Event));
    return event;
}

static void eventQueueWindowPosCallback(GLFWwindow* window, int x, int y)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_MOVED;
    event->window = window;
    event->pos.x = x;
    event->pos.y = y;
}

static void eventQueueWindowSizeCallback(GLFWwindow* window, int width, int height)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_RESIZED;
    event->window = window;
    event->size.width = width;
    event->size.height = height;
}

static void eventQueueWindowCloseCallback(GLFWwindow* window)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_CLOSED;
    event->window = window;
}

static void eventQueueWindowRefreshCallback(GLFWwindow* window)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_REFRESH;
    event->window = window;
}

static void eventQueueWindowFocusCallback(GLFWwindow* window, int focused)
{
    Event* event = eventQueueNewEvent();
    event->window = window;

    if (focused)
        event->type = EVENT_WINDOW_FOCUSED;
    else
        event->type = EVENT_WINDOW_DEFOCUSED;
}

static void eventQueueWindowIconifyCallback(GLFWwindow* window, int iconified)
{
    Event* event = eventQueueNewEvent();
    event->window = window;

    if (iconified)
        event->type = EVENT_WINDOW_ICONIFIED;
    else
        event->type = EVENT_WINDOW_UNICONIFIED;
}

static void eventQueueFramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_FRAMEBUFFER_RESIZED;
    event->window = window;
    event->size.width = width;
    event->size.height = height;
}

static void eventQueueMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Event* event = eventQueueNewEvent();
    event->window = window;
    event->mouse.button = button;
    event->mouse.mods = mods;

    if (action == GLFW_PRESS)
        event->type = EVENT_BUTTON_PRESSED;
    else if (action == GLFW_RELEASE)
        event->type = EVENT_BUTTON_RELEASED;
}

static void eventQueueCursorPosCallback(GLFWwindow* window, double x, double y)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_CURSOR_MOVED;
    event->window = window;
    event->pos.x = (int) x;
    event->pos.y = (int) y;
}

static void eventQueueCursorEnterCallback(GLFWwindow* window, int entered)
{
    Event* event = eventQueueNewEvent();
    event->window = window;

    if (entered)
        event->type = EVENT_CURSOR_ENTERED;
    else
        event->type = EVENT_CURSOR_LEFT;
}

static void eventQueueScrollCallback(GLFWwindow* window, double x, double y)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_SCROLLED;
    event->window = window;
    event->scroll.x = x;
    event->scroll.y = y;
}

static void eventQueueKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Event* event = eventQueueNewEvent();
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

static void eventQueueCharCallback(GLFWwindow* window, unsigned int codepoint)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_CODEPOINT_INPUT;
    event->window = window;
    event->codepoint = codepoint;
}

static void eventQueueMonitorCallback(GLFWmonitor* monitor, int action)
{
    Event* event = eventQueueNewEvent();
    event->monitor = monitor;

    if (action == GLFW_CONNECTED)
        event->type = EVENT_MONITOR_CONNECTED;
    else if (action == GLFW_DISCONNECTED)
        event->type = EVENT_MONITOR_DISCONNECTED;
}

static void eventQueueFileDropCallback(GLFWwindow* window, int count, const char** paths)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_FILE_DROPPED;
    event->window = window;
    event->file.paths = (char**) malloc(count * sizeof(char*));
    event->file.count = count;

    while (count--)
        event->file.paths[count] = eventQueueStrdup(paths[count]);
}

static void eventQueueJoystickCallback(int jid, int action)
{
    Event* event = eventQueueNewEvent();
    event->joystick = jid;

    if (action == GLFW_CONNECTED)
        event->type = EVENT_JOYSTICK_CONNECTED;
    else if (action == GLFW_DISCONNECTED)
        event->type = EVENT_JOYSTICK_DISCONNECTED;
}

static void eventQueueWindowMaximizeCallback(GLFWwindow* window, int maximized)
{
    Event* event = eventQueueNewEvent();
    event->window = window;

    if (maximized)
        event->type = EVENT_WINDOW_MAXIMIZED;
    else
        event->type = EVENT_WINDOW_UNMAXIMIZED;
}

static void eventQueueWindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale)
{
    Event* event = eventQueueNewEvent();
    event->window = window;
    event->type = EVENT_WINDOW_SCALE_CHANGED;
    event->scale.x = xscale;
    event->scale.y = yscale;
}

static inline
void eventFree(Event* event)
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

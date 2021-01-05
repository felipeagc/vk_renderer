#include "platform.hpp"

#include <rg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <tinyshader/tinyshader.h>

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

struct PlatformImpl
{
    GLFWwindow *window;
    RgDevice *device;
    RgSwapchain *swapchain;
    const char *exe_dir;

    void resizeResources();
};

void PlatformImpl::resizeResources()
{
    int width, height;
    glfwGetFramebufferSize(this->window, &width, &height);

    RgSwapchain *old_swapchain = this->swapchain;

    RgSwapchainInfo swapchain_info = {};
    swapchain_info.vsync = true;
    swapchain_info.old_swapchain = old_swapchain;

    swapchain_info.width = width;
    swapchain_info.height = height;

#ifdef _WIN32
    swapchain_info.window_handle = (void*)glfwGetWin32Window(this->window),
#else
    swapchain_info.window_handle = (void*)glfwGetX11Window(this->window),
    swapchain_info.display_handle = (void*)glfwGetX11Display(),
#endif

    this->swapchain = rgSwapchainCreate(this->device, &swapchain_info);

    if (old_swapchain)
    {
        rgSwapchainDestroy(this->device, old_swapchain);
    }
}

Platform *Platform::create(const char *window_title)
{
    Platform *platform = new Platform();
    platform->impl = new PlatformImpl();

    platform->impl->exe_dir = getExeDirPath();

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

    platform->impl->window = window;

    RgDeviceInfo device_info = {};
    device_info.enable_validation = true;
    platform->impl->device = rgDeviceCreate(&device_info);

    platform->impl->resizeResources();

    return platform;
}

void Platform::destroy()
{
    rgSwapchainDestroy(this->impl->device, this->impl->swapchain);
    rgDeviceDestroy(this->impl->device);

    glfwDestroyWindow(this->impl->window);
    glfwTerminate();

    delete[] this->impl->exe_dir;

    delete this->impl;
    delete this;
}

const char *Platform::getExeDir()
{
    return this->impl->exe_dir;
}

void Platform::getWindowSize(uint32_t *width, uint32_t *height)
{
    int iwidth, iheight;
    glfwGetFramebufferSize(this->impl->window, &iwidth, &iheight);
    *width = (uint32_t)iwidth;
    *height = (uint32_t)iheight;
}

RgDevice *Platform::getDevice()
{
    return this->impl->device;
}

RgSwapchain *Platform::getSwapchain()
{
    return this->impl->swapchain;
}


bool Platform::shouldClose()
{
    return glfwWindowShouldClose(this->impl->window);
}

void Platform::pollEvents()
{
    glfwPollEvents();
}

bool Platform::nextEvent(Event *event)
{
    memset(event, 0, sizeof(Event));

    if (event_queue.head != event_queue.tail)
    {
        *event = event_queue.events[event_queue.tail];
        event_queue.tail = (event_queue.tail + 1) % EVENT_CAPACITY;
    }

    if (event->type == EVENT_WINDOW_RESIZED)
    {
        this->impl->resizeResources();
    }

    return event->type != EVENT_NONE;
}

uint8_t *Platform::loadFileRelative(const char *relative_path, size_t *size)
{
    size_t path_size = strlen(this->impl->exe_dir) + 1 + strlen(relative_path) + 1;
    char *path = new char[path_size];
    snprintf(path, path_size, "%s/%s", this->impl->exe_dir, relative_path);
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

RgPipeline *Platform::createPipeline(const char *hlsl, size_t hlsl_size)
{
    uint8_t *vertex_code = NULL;
    size_t vertex_code_size = 0;

    // Compile vertex
    {
        TsCompilerOptions *options = tsCompilerOptionsCreate();
        tsCompilerOptionsSetStage(options, TS_SHADER_STAGE_VERTEX);
        tsCompilerOptionsSetEntryPoint(options, "vertex", strlen("vertex"));
        tsCompilerOptionsSetSource(options, hlsl, hlsl_size, NULL, 0);

        TsCompilerOutput *output = tsCompile(options);
        const char *errors = tsCompilerOutputGetErrors(output);
        if (errors)
        {
            printf("Shader compilation error:\n%s\n", errors);

            tsCompilerOutputDestroy(output);
            tsCompilerOptionsDestroy(options);
            exit(1);
        }

        size_t spirv_size = 0;
        const uint8_t *spirv = tsCompilerOutputGetSpirv(output, &spirv_size);

        vertex_code = new uint8_t[spirv_size];
        memcpy(vertex_code, spirv, spirv_size);
        vertex_code_size = spirv_size;

        tsCompilerOutputDestroy(output);
        tsCompilerOptionsDestroy(options);
    }

    uint8_t *fragment_code = NULL;
    size_t fragment_code_size = 0;

    // Compile fragment
    {
        TsCompilerOptions *options = tsCompilerOptionsCreate();
        tsCompilerOptionsSetStage(options, TS_SHADER_STAGE_FRAGMENT);
        tsCompilerOptionsSetEntryPoint(options, "pixel", strlen("pixel"));
        tsCompilerOptionsSetSource(options, hlsl, hlsl_size, NULL, 0);

        TsCompilerOutput *output = tsCompile(options);
        const char *errors = tsCompilerOutputGetErrors(output);
        if (errors)
        {
            printf("Shader compilation error:\n%s\n", errors);

            tsCompilerOutputDestroy(output);
            tsCompilerOptionsDestroy(options);
            exit(1);
        }

        size_t spirv_size = 0;
        const uint8_t *spirv = tsCompilerOutputGetSpirv(output, &spirv_size);

        fragment_code = new uint8_t[spirv_size];
        memcpy(fragment_code, spirv, spirv_size);
        fragment_code_size = spirv_size;

        tsCompilerOutputDestroy(output);
        tsCompilerOptionsDestroy(options);
    }

    RgGraphicsPipelineInfo pipeline_info = {
        .polygon_mode = RG_POLYGON_MODE_FILL,
        .cull_mode = RG_CULL_MODE_NONE,
        .front_face = RG_FRONT_FACE_CLOCKWISE,
        .topology = RG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .blend = { .enable = false },

        .vertex = vertex_code,
        .vertex_size = vertex_code_size,
        .vertex_entry = "vertex",

        .fragment = fragment_code,
        .fragment_size = fragment_code_size,
        .fragment_entry = "pixel",
    };

    const char *pragma = "#pragma";
    size_t pragma_len = strlen(pragma);

    auto isWhitespace = [](char c) {
        return c == ' ' || c == '\t';
    };

    auto isWhitespaceOrNewLine = [](char c) {
        return c == ' ' || c == '\r' || c == '\n' || c == '\t';
    };

    auto stringToBool = [](const char *str, size_t len, bool *value) -> bool {
        if (strncmp(str, "true", len) == 0) *value = true;
        else if (strncmp(str, "false", len) == 0) *value = false;
        else return false;
        return true;
    };

    auto stringToTopology = [](const char *str, size_t len, RgPrimitiveTopology *value) -> bool {
        if (strncmp(str, "triangle_list", len) == 0) *value = RG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        else if (strncmp(str, "line_list", len) == 0) *value = RG_PRIMITIVE_TOPOLOGY_LINE_LIST;
        else return false;
        return true;
    };

    auto stringToFrontFace = [](const char *str, size_t len, RgFrontFace *value) -> bool {
        if (strncmp(str, "counter_clockwise", len) == 0) *value = RG_FRONT_FACE_COUNTER_CLOCKWISE;
        else if (strncmp(str, "clockwise", len) == 0) *value = RG_FRONT_FACE_CLOCKWISE;
        else return false;
        return true;
    };

    auto stringToCullMode = [](const char *str, size_t len, RgCullMode * value) -> bool {
        if (strncmp(str, "none", len) == 0) *value =  RG_CULL_MODE_NONE;
        else if (strncmp(str, "front", len) == 0) *value =  RG_CULL_MODE_FRONT;
        else if (strncmp(str, "back", len) == 0) *value =  RG_CULL_MODE_BACK;
        else if (strncmp(str, "front_and_back", len) == 0) *value = RG_CULL_MODE_FRONT_AND_BACK;
        else return false;
        return true;
    };

    auto stringToPolygonMode = [](const char *str, size_t len, RgPolygonMode *value) -> bool {
        if (strncmp(str, "fill", len) == 0) *value = RG_POLYGON_MODE_FILL;
        else if (strncmp(str, "line", len) == 0) *value = RG_POLYGON_MODE_LINE;
        else if (strncmp(str, "point", len) == 0) *value = RG_POLYGON_MODE_POINT;
        else return false;
        return true;
    };

    for (size_t i = 0; i < hlsl_size; ++i)
    {
        size_t len = hlsl_size - i;
        if (hlsl[i] == '#' && len > pragma_len && strncmp(&hlsl[i], pragma, pragma_len) == 0)
        {
            i += pragma_len;
            while (isWhitespace(hlsl[i])) i++;

            size_t key_start = i;
            while (!isWhitespaceOrNewLine(hlsl[i])) i++;
            size_t key_end = i;

            while (isWhitespace(hlsl[i])) i++;

            size_t value_start = i;
            while (!isWhitespaceOrNewLine(hlsl[i])) i++;
            size_t value_end = i;

            const char *key = &hlsl[key_start];
            size_t key_len = key_end - key_start;

            const char *value = &hlsl[value_start];
            size_t value_len = value_end - value_start;

            bool success = true;
            if (strncmp(key, "blend", key_len) == 0)
            {
                 success = stringToBool(value, value_len, &pipeline_info.blend.enable);
            }
            else if (strncmp(key, "depth_test", key_len) == 0)
            {
                 success = stringToBool(value, value_len, &pipeline_info.depth_stencil.test_enable);
            }
            else if (strncmp(key, "depth_write", key_len) == 0)
            {
                 success = stringToBool(value, value_len, &pipeline_info.depth_stencil.write_enable);
            }
            else if (strncmp(key, "depth_bias", key_len) == 0)
            {
                success = stringToBool(value, value_len, &pipeline_info.depth_stencil.bias_enable);
            }
            else if (strncmp(key, "topology", key_len) == 0)
            {
                 success = stringToTopology(value, value_len, &pipeline_info.topology);
            }
            else if (strncmp(key, "polygon_mode", key_len) == 0)
            {
                success = stringToPolygonMode(value, value_len, &pipeline_info.polygon_mode);
            }
            else if (strncmp(key, "cull_mode", key_len) == 0)
            {
                success = stringToCullMode(value, value_len, &pipeline_info.cull_mode);
            }
            else if (strncmp(key, "front_face", key_len) == 0)
            {
                success = stringToFrontFace(value, value_len, &pipeline_info.front_face);
            }
            else
            {
                success = false;
            }

            if (!success)
            {
                fprintf(stderr, "Warning: invalid pipeline parameter: '%.*s': '%.*s'\n",
                        (int)key_len, key, (int)value_len, value);
            }
        }
    }

    RgPipeline *pipeline = rgExtGraphicsPipelineCreateInferredBindings(
                this->impl->device,
                true,
                &pipeline_info);

    delete[] vertex_code;
    delete[] fragment_code;

    return pipeline;
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
    /* event->window = window; */
    event->pos.x = x;
    event->pos.y = y;
}

static void eventQueueWindowSizeCallback(GLFWwindow* window, int width, int height)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_RESIZED;
    /* event->window = window; */
    event->size.width = width;
    event->size.height = height;
}

static void eventQueueWindowCloseCallback(GLFWwindow* window)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_CLOSED;
    /* event->window = window; */
}

static void eventQueueWindowRefreshCallback(GLFWwindow* window)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_WINDOW_REFRESH;
    /* event->window = window; */
}

static void eventQueueWindowFocusCallback(GLFWwindow* window, int focused)
{
    Event* event = eventQueueNewEvent();
    /* event->window = window; */

    if (focused)
        event->type = EVENT_WINDOW_FOCUSED;
    else
        event->type = EVENT_WINDOW_DEFOCUSED;
}

static void eventQueueWindowIconifyCallback(GLFWwindow* window, int iconified)
{
    Event* event = eventQueueNewEvent();
    /* event->window = window; */

    if (iconified)
        event->type = EVENT_WINDOW_ICONIFIED;
    else
        event->type = EVENT_WINDOW_UNICONIFIED;
}

static void eventQueueFramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_FRAMEBUFFER_RESIZED;
    /* event->window = window; */
    event->size.width = width;
    event->size.height = height;
}

static void eventQueueMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Event* event = eventQueueNewEvent();
    /* event->window = window; */
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
    /* event->window = window; */
    event->pos.x = (int) x;
    event->pos.y = (int) y;
}

static void eventQueueCursorEnterCallback(GLFWwindow* window, int entered)
{
    Event* event = eventQueueNewEvent();
    /* event->window = window; */

    if (entered)
        event->type = EVENT_CURSOR_ENTERED;
    else
        event->type = EVENT_CURSOR_LEFT;
}

static void eventQueueScrollCallback(GLFWwindow* window, double x, double y)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_SCROLLED;
    /* event->window = window; */
    event->scroll.x = x;
    event->scroll.y = y;
}

static void eventQueueKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Event* event = eventQueueNewEvent();
    /* event->window = window; */
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
    /* event->window = window; */
    event->codepoint = codepoint;
}

static void eventQueueMonitorCallback(GLFWmonitor* monitor, int action)
{
    Event* event = eventQueueNewEvent();
    /* event->monitor = monitor; */

    if (action == GLFW_CONNECTED)
        event->type = EVENT_MONITOR_CONNECTED;
    else if (action == GLFW_DISCONNECTED)
        event->type = EVENT_MONITOR_DISCONNECTED;
}

static void eventQueueFileDropCallback(GLFWwindow* window, int count, const char** paths)
{
    Event* event = eventQueueNewEvent();
    event->type = EVENT_FILE_DROPPED;
    /* event->window = window; */
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
    /* event->window = window; */

    if (maximized)
        event->type = EVENT_WINDOW_MAXIMIZED;
    else
        event->type = EVENT_WINDOW_UNMAXIMIZED;
}

static void eventQueueWindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale)
{
    Event* event = eventQueueNewEvent();
    /* event->window = window; */
    event->type = EVENT_WINDOW_SCALE_CHANGED;
    event->scale.x = xscale;
    event->scale.y = yscale;
}

static void eventFree(Event* event)
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

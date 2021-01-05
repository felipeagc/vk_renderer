#pragma once

#include <rg.h>
#include <rg_ext.h>

struct Event;

struct Platform
{
    static Platform *create(const char *window_title);
    void destroy();

    const char* getExeDir();
    void getWindowSize(uint32_t *width, uint32_t *height);
    RgDevice *getDevice();
    RgSwapchain *getSwapchain();

    bool shouldClose();
    void pollEvents();
    bool nextEvent(Event *event);

    uint8_t *loadFileRelative(const char *relative_path, size_t *size);
    RgPipeline *createPipeline(const char *hlsl, size_t hlsl_size);

private:
    struct PlatformImpl *impl;
};

enum EventType
{
    EVENT_NONE,
    EVENT_WINDOW_MOVED,
    EVENT_WINDOW_RESIZED,
    EVENT_WINDOW_CLOSED,
    EVENT_WINDOW_REFRESH,
    EVENT_WINDOW_FOCUSED,
    EVENT_WINDOW_DEFOCUSED,
    EVENT_WINDOW_ICONIFIED,
    EVENT_WINDOW_UNICONIFIED,
    EVENT_FRAMEBUFFER_RESIZED,
    EVENT_BUTTON_PRESSED,
    EVENT_BUTTON_RELEASED,
    EVENT_CURSOR_MOVED,
    EVENT_CURSOR_ENTERED,
    EVENT_CURSOR_LEFT,
    EVENT_SCROLLED,
    EVENT_KEY_PRESSED,
    EVENT_KEY_REPEATED,
    EVENT_KEY_RELEASED,
    EVENT_CODEPOINT_INPUT,
    EVENT_MONITOR_CONNECTED,
    EVENT_MONITOR_DISCONNECTED,
    EVENT_FILE_DROPPED,
    EVENT_JOYSTICK_CONNECTED,
    EVENT_JOYSTICK_DISCONNECTED,
    EVENT_WINDOW_MAXIMIZED,
    EVENT_WINDOW_UNMAXIMIZED,
    EVENT_WINDOW_SCALE_CHANGED,
};

struct Event
{
    EventType type;
    union {
        /* GLFWwindow* window; */
        /* GLFWmonitor* monitor; */
        int joystick;
    };
    union {
        struct {
            int x;
            int y;
        } pos;
        struct {
            int width;
            int height;
        } size;
        struct {
            double x;
            double y;
        } scroll;
        struct {
            int key;
            int scancode;
            int mods;
        } keyboard;
        struct {
            int button;
            int mods;
        } mouse;
        unsigned int codepoint;
        struct {
            char** paths;
            int count;
        } file;
        struct {
            float x;
            float y;
        } scale;
    };
};

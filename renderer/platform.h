#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct RgDevice RgDevice;
typedef struct RgSwapchain RgSwapchain;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;

typedef enum EventType
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
} EventType;

typedef struct Event
{
    EventType type;
    union {
        void* window;
        void* monitor;
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
} Event;

typedef enum KeyMod
{
    KEY_MOD_SHIFT = 0X0001,
    KEY_MOD_CONTROL = 0X0002,
    KEY_MOD_ALT = 0X0004,
    KEY_MOD_SUPER = 0X0008,
    KEY_MOD_CAPSLOCK = 0X0010,
    KEY_MOD_NUMLOCK = 0X0020,
} KeyMod;

typedef enum Button
{
    BUTTON_LEFT = 0,
    BUTTON_RIGHT = 1,
    BUTTON_MIDDLE = 2,
    BUTTON_BUTTON4 = 3,
    BUTTON_BUTTON5 = 4,
    BUTTON_BUTTON6 = 5,
    BUTTON_BUTTON7 = 6,
    BUTTON_BUTTON8 = 7,
} Button;

typedef enum Key
{
    KEY_SPACE        = 32,
    KEY_APOSTROPHE   = 39,
    KEY_COMMA        = 44,
    KEY_MINUS        = 45,
    KEY_PERIOD       = 46,
    KEY_SLASH        = 47,
    KEY_NUMBER0      = 48,
    KEY_NUMBER1      = 49,
    KEY_NUMBER2      = 50,
    KEY_NUMBER3      = 51,
    KEY_NUMBER4      = 52,
    KEY_NUMBER5      = 53,
    KEY_NUMBER6      = 54,
    KEY_NUMBER7      = 55,
    KEY_NUMBER8      = 56,
    KEY_NUMBER9      = 57,
    KEY_SEMICOLON    = 59,
    KEY_EQUAL        = 61,
    KEY_A            = 65,
    KEY_B            = 66,
    KEY_C            = 67,
    KEY_D            = 68,
    KEY_E            = 69,
    KEY_F            = 70,
    KEY_G            = 71,
    KEY_H            = 72,
    KEY_I            = 73,
    KEY_J            = 74,
    KEY_K            = 75,
    KEY_L            = 76,
    KEY_M            = 77,
    KEY_N            = 78,
    KEY_O            = 79,
    KEY_P            = 80,
    KEY_Q            = 81,
    KEY_R            = 82,
    KEY_S            = 83,
    KEY_T            = 84,
    KEY_U            = 85,
    KEY_V            = 86,
    KEY_W            = 87,
    KEY_X            = 88,
    KEY_Y            = 89,
    KEY_Z            = 90,
    KEY_LEFTBRACKET  = 91, 
    KEY_BACKSLASH    = 92,
    KEY_RIGHTBRACKET = 93,
    KEY_GRAVEACCENT  = 96,
    KEY_WORLD1       = 161,
    KEY_WORLD2       = 162,

    KEY_ESCAPE       = 256,
    KEY_ENTER        = 257,
    KEY_TAB          = 258,
    KEY_BACKSPACE    = 259,
    KEY_INSERT       = 260,
    KEY_DELETE       = 261,
    KEY_RIGHT        = 262,
    KEY_LEFT         = 263,
    KEY_DOWN         = 264,
    KEY_UP           = 265,
    KEY_PAGEUP       = 266,
    KEY_PAGEDOWN     = 267,
    KEY_HOME         = 268,
    KEY_END          = 269,
    KEY_CAPSLOCK     = 280,
    KEY_SCROLLLOCK   = 281,
    KEY_NUMLOCK      = 282,
    KEY_PRINTSCREEN  = 283,
    KEY_PAUSE        = 284,
    KEY_F1           = 290,
    KEY_F2           = 291,
    KEY_F3           = 292,
    KEY_F4           = 293,
    KEY_F5           = 294,
    KEY_F6           = 295,
    KEY_F7           = 296,
    KEY_F8           = 297,
    KEY_F9           = 298,
    KEY_F10          = 299,
    KEY_F11          = 300,
    KEY_F12          = 301,
    KEY_F13          = 302,
    KEY_F14          = 303,
    KEY_F15          = 304,
    KEY_F16          = 305,
    KEY_F17          = 306,
    KEY_F18          = 307,
    KEY_F19          = 308,
    KEY_F20          = 309,
    KEY_F21          = 310,
    KEY_F22          = 311,
    KEY_F23          = 312,
    KEY_F24          = 313,
    KEY_F25          = 314,
    KEY_KP0          = 320,
    KEY_KP1          = 321,
    KEY_KP2          = 322,
    KEY_KP3          = 323,
    KEY_KP4          = 324,
    KEY_KP5          = 325,
    KEY_KP6          = 326,
    KEY_KP7          = 327,
    KEY_KP8          = 328,
    KEY_KP9          = 329,
    KEY_KPDECIMAL    = 330,
    KEY_KPDIVIDE     = 331,
    KEY_KPMULTIPLY   = 332,
    KEY_KPSUBTRACT   = 333,
    KEY_KPADD        = 334,
    KEY_KPENTER      = 335,
    KEY_KPEQUAL      = 336,
    KEY_LEFTSHIFT    = 340,
    KEY_LEFTCONTROL  = 341,
    KEY_LEFTALT      = 342,
    KEY_LEFTSUPER    = 343,
    KEY_RIGHTSHIFT   = 344,
    KEY_RIGHTCONTROL = 345,
    KEY_RIGHTALT     = 346,
    KEY_RIGHTSUPER   = 347,
    KEY_MENU         = 348,
} Key;

typedef struct Platform Platform;

Platform *PlatformCreate(Allocator *allocator, const char *title);
void PlatformDestroy(Platform *platform);

RgDevice *PlatformGetDevice(Platform *platform);
RgSwapchain *PlatformGetSwapchain(Platform *platform);

double PlatformGetTime(Platform *platform);
void PlatformGetWindowSize(Platform *platform, uint32_t *width, uint32_t *height);
bool PlatformGetCursorEnabled(Platform *platform);
void PlatformSetCursorEnabled(Platform *platform, bool enabled);
void PlatformGetCursorPos(Platform *platform, double *x, double *y);
bool PlatformGetKeyState(Platform *platform, Key key);
bool PlatformGetButtonState(Platform *platform, Button button);

bool PlatformShouldClose(Platform *platform);
void PlatformPollEvents(Platform *platform);
bool PlatformNextEvent(Platform *platform, Event *event);

#ifdef __cplusplus
}
#endif

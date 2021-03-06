#pragma once

#include "base.h"
#include "math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;
typedef struct RgCmdPool RgCmdPool;
typedef struct RgBuffer RgBuffer;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct RgDescriptorSet RgDescriptorSet;
typedef struct RgPipelineLayout RgPipelineLayout;
typedef struct RgPipeline RgPipeline;
typedef struct RgBufferInfo RgBufferInfo;
typedef struct RgImageInfo RgImageInfo;
typedef struct RgSamplerInfo RgSamplerInfo;
typedef struct RgDevice RgDevice;
typedef struct RgSwapchain RgSwapchain;

typedef struct EgEngine EgEngine;

typedef struct EgVertex
{
    float3 pos;
    float3 normal;
    float tangent[4];
    float2 uv;
} EgVertex;

typedef struct EgImage
{
	RgImage *image;
	uint32_t index;
} EgImage;

typedef struct EgSampler
{
	RgSampler *sampler;
	uint32_t index;
} EgSampler;

typedef struct EgBuffer
{
	RgBuffer *buffer;
	uint32_t index;
} EgBuffer;

// Events {{{
typedef enum EgEventType
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
} EgEventType;

typedef struct EgEvent
{
    EgEventType type;
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
} EgEvent;

typedef enum EgKeyMod
{
    KEY_MOD_SHIFT = 0X0001,
    KEY_MOD_CONTROL = 0X0002,
    KEY_MOD_ALT = 0X0004,
    KEY_MOD_SUPER = 0X0008,
    KEY_MOD_CAPSLOCK = 0X0010,
    KEY_MOD_NUMLOCK = 0X0020,
} EgKeyMod;

typedef enum EgButton
{
    EG_BUTTON_LEFT = 0,
    EG_BUTTON_RIGHT = 1,
    EG_BUTTON_MIDDLE = 2,
    EG_BUTTON_BUTTON4 = 3,
    EG_BUTTON_BUTTON5 = 4,
    EG_BUTTON_BUTTON6 = 5,
    EG_BUTTON_BUTTON7 = 6,
    EG_BUTTON_BUTTON8 = 7,
} EgButton;

typedef enum EgKey
{
    EG_KEY_SPACE        = 32,
    EG_KEY_APOSTROPHE   = 39,
    EG_KEY_COMMA        = 44,
    EG_KEY_MINUS        = 45,
    EG_KEY_PERIOD       = 46,
    EG_KEY_SLASH        = 47,
    EG_KEY_NUMBER0      = 48,
    EG_KEY_NUMBER1      = 49,
    EG_KEY_NUMBER2      = 50,
    EG_KEY_NUMBER3      = 51,
    EG_KEY_NUMBER4      = 52,
    EG_KEY_NUMBER5      = 53,
    EG_KEY_NUMBER6      = 54,
    EG_KEY_NUMBER7      = 55,
    EG_KEY_NUMBER8      = 56,
    EG_KEY_NUMBER9      = 57,
    EG_KEY_SEMICOLON    = 59,
    EG_KEY_EQUAL        = 61,
    EG_KEY_A            = 65,
    EG_KEY_B            = 66,
    EG_KEY_C            = 67,
    EG_KEY_D            = 68,
    EG_KEY_E            = 69,
    EG_KEY_F            = 70,
    EG_KEY_G            = 71,
    EG_KEY_H            = 72,
    EG_KEY_I            = 73,
    EG_KEY_J            = 74,
    EG_KEY_K            = 75,
    EG_KEY_L            = 76,
    EG_KEY_M            = 77,
    EG_KEY_N            = 78,
    EG_KEY_O            = 79,
    EG_KEY_P            = 80,
    EG_KEY_Q            = 81,
    EG_KEY_R            = 82,
    EG_KEY_S            = 83,
    EG_KEY_T            = 84,
    EG_KEY_U            = 85,
    EG_KEY_V            = 86,
    EG_KEY_W            = 87,
    EG_KEY_X            = 88,
    EG_KEY_Y            = 89,
    EG_KEY_Z            = 90,
    EG_KEY_LEFTBRACKET  = 91, 
    EG_KEY_BACKSLASH    = 92,
    EG_KEY_RIGHTBRACKET = 93,
    EG_KEY_GRAVEACCENT  = 96,
    EG_KEY_WORLD1       = 161,
    EG_KEY_WORLD2       = 162,

    EG_KEY_ESCAPE       = 256,
    EG_KEY_ENTER        = 257,
    EG_KEY_TAB          = 258,
    EG_KEY_BACKSPACE    = 259,
    EG_KEY_INSERT       = 260,
    EG_KEY_DELETE       = 261,
    EG_KEY_RIGHT        = 262,
    EG_KEY_LEFT         = 263,
    EG_KEY_DOWN         = 264,
    EG_KEY_UP           = 265,
    EG_KEY_PAGEUP       = 266,
    EG_KEY_PAGEDOWN     = 267,
    EG_KEY_HOME         = 268,
    EG_KEY_END          = 269,
    EG_KEY_CAPSLOCK     = 280,
    EG_KEY_SCROLLLOCK   = 281,
    EG_KEY_NUMLOCK      = 282,
    EG_KEY_PRINTSCREEN  = 283,
    EG_KEY_PAUSE        = 284,
    EG_KEY_F1           = 290,
    EG_KEY_F2           = 291,
    EG_KEY_F3           = 292,
    EG_KEY_F4           = 293,
    EG_KEY_F5           = 294,
    EG_KEY_F6           = 295,
    EG_KEY_F7           = 296,
    EG_KEY_F8           = 297,
    EG_KEY_F9           = 298,
    EG_KEY_F10          = 299,
    EG_KEY_F11          = 300,
    EG_KEY_F12          = 301,
    EG_KEY_F13          = 302,
    EG_KEY_F14          = 303,
    EG_KEY_F15          = 304,
    EG_KEY_F16          = 305,
    EG_KEY_F17          = 306,
    EG_KEY_F18          = 307,
    EG_KEY_F19          = 308,
    EG_KEY_F20          = 309,
    EG_KEY_F21          = 310,
    EG_KEY_F22          = 311,
    EG_KEY_F23          = 312,
    EG_KEY_F24          = 313,
    EG_KEY_F25          = 314,
    EG_KEY_KP0          = 320,
    EG_KEY_KP1          = 321,
    EG_KEY_KP2          = 322,
    EG_KEY_KP3          = 323,
    EG_KEY_KP4          = 324,
    EG_KEY_KP5          = 325,
    EG_KEY_KP6          = 326,
    EG_KEY_KP7          = 327,
    EG_KEY_KP8          = 328,
    EG_KEY_KP9          = 329,
    EG_KEY_KPDECIMAL    = 330,
    EG_KEY_KPDIVIDE     = 331,
    EG_KEY_KPMULTIPLY   = 332,
    EG_KEY_KPSUBTRACT   = 333,
    EG_KEY_KPADD        = 334,
    EG_KEY_KPENTER      = 335,
    EG_KEY_KPEQUAL      = 336,
    EG_KEY_LEFTSHIFT    = 340,
    EG_KEY_LEFTCONTROL  = 341,
    EG_KEY_LEFTALT      = 342,
    EG_KEY_LEFTSUPER    = 343,
    EG_KEY_RIGHTSHIFT   = 344,
    EG_KEY_RIGHTCONTROL = 345,
    EG_KEY_RIGHTALT     = 346,
    EG_KEY_RIGHTSUPER   = 347,
    EG_KEY_MENU         = 348,
} EgKey;
// }}}

EgEngine *egEngineCreate(EgAllocator *allocator);
void egEngineDestroy(EgEngine *engine);
RgDevice *egEngineGetDevice(EgEngine *engine);
RgSwapchain *egEngineGetSwapchain(EgEngine *engine);

double egEngineGetTime(EgEngine *platform);
void egEngineGetWindowSize(EgEngine *platform, uint32_t *width, uint32_t *height);
bool egEngineGetCursorEnabled(EgEngine *platform);
void egEngineSetCursorEnabled(EgEngine *platform, bool enabled);
void egEngineGetCursorPos(EgEngine *platform, double *x, double *y);
bool egEngineGetKeyState(EgEngine *platform, EgKey key);
bool egEngineGetButtonState(EgEngine *platform, EgButton button);

bool egEngineShouldClose(EgEngine *platform);
void egEnginePollEvents(EgEngine *platform);
bool egEngineNextEvent(EgEngine *platform, EgEvent *event);

const char *egEngineGetExeDir(EgEngine *engine);

// Loads a file relative to the executable path
uint8_t *
egEngineLoadFileRelative(EgEngine *engine, EgAllocator *allocator, const char *relative_path, size_t *size);

RgCmdPool *egEngineGetTransferCmdPool(EgEngine *engine);
EgImage egEngineGetWhiteImage(EgEngine *engine);
EgImage egEngineGetBlackImage(EgEngine *engine);
EgSampler egEngineGetDefaultSampler(EgEngine *engine);
EgImage egEngineGetBRDFImage(EgEngine *engine);

RgPipeline *egEngineCreateGraphicsPipeline(EgEngine *engine, const char *path);
RgPipeline *egEngineCreateComputePipeline(EgEngine *engine, const char *path);

RgPipelineLayout *egEngineGetGlobalPipelineLayout(EgEngine *engine);
RgDescriptorSet *egEngineGetGlobalDescriptorSet(EgEngine *engine);

EgBuffer egEngineAllocateStorageBuffer(EgEngine *engine, RgBufferInfo *info);
void egEngineFreeStorageBuffer(EgEngine *engine, EgBuffer *handle);

EgImage egEngineAllocateImage(EgEngine *engine, RgImageInfo *info);
void egEngineFreeImage(EgEngine *engine, EgImage *handle);

EgSampler egEngineAllocateSampler(EgEngine *engine, RgSamplerInfo *info);
void egEngineFreeSampler(EgEngine *engine, EgSampler *handle);

#ifdef __cplusplus
}
#endif

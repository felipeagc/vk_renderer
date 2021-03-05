#include "camera.h"

#include "math.h"
#include "platform.h"
#include "engine.h"

extern "C" void FPSCameraInit(FPSCamera *camera, Engine *engine)
{
    *camera = {};
    camera->engine = engine;
    camera->pos = V3(0.0f, 0.0f, 0.0f);
    camera->yaw = EG_RADIANS(180.0f);
    camera->pitch = 0.0f;
    camera->fovy = EG_RADIANS(75.0f);
    camera->prev_x = 0;
    camera->prev_y = 0;
    camera->sensitivity = 0.14f;
    camera->speed = 1.0f;
}

extern "C" CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time)
{
    Platform *platform = EngineGetPlatform(camera->engine);

    if (!PlatformGetCursorEnabled(platform))
    {
        double cx, cy;
        PlatformGetCursorPos(platform, &cx, &cy);

        float dx = (float)(cx - camera->prev_x);
        float dy = (float)(cy - camera->prev_y);

        camera->prev_x = cx;
        camera->prev_y = cy;

        camera->yaw -= EG_RADIANS(dx * camera->sensitivity);
        camera->pitch -= EG_RADIANS(dy * camera->sensitivity);
        camera->pitch = EG_CLAMP(camera->pitch, EG_RADIANS(-89.0f), EG_RADIANS(89.0f));
    }

    float3 front = eg_float3_normalize(
        V3(sinf(camera->yaw) * cosf(camera->pitch),
           sinf(camera->pitch),
           cosf(camera->yaw) * cosf(camera->pitch)));

    float3 right = eg_float3_cross(front, V3(0.0f, 1.0f, 0.0f));
    float3 up = eg_float3_cross(right, front);

    float delta = camera->speed * delta_time;

    float3 forward_increment = eg_float3_mul_scalar(front, delta);
    float3 right_increment = eg_float3_mul_scalar(right, delta);

    if (PlatformGetKeyState(platform, KEY_W))
    {
        camera->pos = eg_float3_add(camera->pos, forward_increment);
    }
    if (PlatformGetKeyState(platform, KEY_S))
    {
        camera->pos = eg_float3_sub(camera->pos, forward_increment);
    }
    if (PlatformGetKeyState(platform, KEY_A))
    {
        camera->pos = eg_float3_sub(camera->pos, right_increment);
    }
    if (PlatformGetKeyState(platform, KEY_D))
    {
        camera->pos = eg_float3_add(camera->pos, right_increment);
    }

    uint32_t width, height;
    PlatformGetWindowSize(platform, &width, &height);

    float4x4 correction_matrix = eg_float4x4_diagonal(1.0f);
    correction_matrix.yy = -1.0f;

    float aspect_ratio = (float)width / (float)height;
    float4x4 proj = eg_float4x4_perspective_reverse_z(camera->fovy, aspect_ratio, 0.1f);
    proj = eg_float4x4_mul(&correction_matrix, &proj);
    float4x4 view =
        eg_float4x4_look_at(camera->pos, eg_float3_add(camera->pos, front), up);

    CameraUniform uniform = {};
    uniform.pos = {
        camera->pos.x,
        camera->pos.y,
        camera->pos.z,
        1.0f,
    };
    uniform.view = view;
    uniform.proj = proj;

    return uniform;
}

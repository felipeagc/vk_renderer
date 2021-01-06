#include "camera.hpp"

void FPSCameraInit(FPSCamera *camera, Platform *platform)
{
    *camera = {};
    camera->platform = platform;
}

CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time)
{
    if (PlatformGetCursorEnabled(camera->platform))
    {
        int32_t cx, cy;
        PlatformGetCursorPos(camera->platform, &cx, &cy);

        float dx = (float)(cx - camera->prev_x);
        float dy = (float)(cy - camera->prev_y);

        camera->prev_x = cx;
        camera->prev_y = cy;

        camera->yaw -= dx * camera->sensitivity * (PI / 180.0);
        camera->pitch -= dy * camera->sensitivity * (PI / 180.0);
        camera->pitch = clamp(camera->pitch, radians(-89.0f), radians(89.0f));
    }

    Vec3 direction = {
        sinf(camera->yaw) * cosf(camera->pitch),
        sinf(camera->pitch),
        cosf(camera->yaw) * cosf(camera->pitch),
    };

    Vec3 front = normalize(direction);
    Vec3 right = cross(front, Vec3(0, -1, 0));
    Vec3 up = cross(right, front);

    float delta = camera->speed * delta_time;
    if (PlatformGetKeyState(camera->platform, KEY_W))
    {
        camera->pos = camera->pos + (front * delta);
    }
    if (PlatformGetKeyState(camera->platform, KEY_S))
    {
        camera->pos = camera->pos - (front * delta);
    }
    if (PlatformGetKeyState(camera->platform, KEY_A))
    {
        camera->pos = camera->pos - (right * delta);
    }
    if (PlatformGetKeyState(camera->platform, KEY_D))
    {
        camera->pos = camera->pos + (right * delta);
    }

    uint32_t width, height;
    PlatformGetWindowSize(camera->platform, &width, &height);

    float aspect_ratio = (float)width / (float)height;
    Mat4 proj = Mat4PerspectiveReverseZ(camera->fovy, aspect_ratio, 0.1f);
    Mat4 view = Mat4LookAt(camera->pos, camera->pos + direction, up);

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

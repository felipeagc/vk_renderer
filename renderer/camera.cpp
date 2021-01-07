#include "camera.h"

void FPSCameraInit(FPSCamera *camera, Platform *platform)
{
    *camera = {};
    camera->platform = platform;
    camera->pos = V3(0.0f, 0.0f, 0.0f);
    camera->yaw = 0.0f;
    camera->pitch = 0.0f;
    camera->fovy = radians(75.0f);
    camera->prev_x = 0;
    camera->prev_y = 0;
    camera->sensitivity = 0.14f;
    camera->speed = 1.0f;
}

CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time)
{
    if (!PlatformGetCursorEnabled(camera->platform))
    {
        double cx, cy;
        PlatformGetCursorPos(camera->platform, &cx, &cy);

        float dx = (float)(cx - camera->prev_x);
        float dy = (float)(cy - camera->prev_y);

        camera->prev_x = cx;
        camera->prev_y = cy;

        camera->yaw -= radians(dx * camera->sensitivity);
        camera->pitch += radians(dy * camera->sensitivity);
        camera->pitch = clamp(camera->pitch, radians(-89.0f), radians(89.0f));
    }

    Vec3 front = normalize(V3(
        sinf(camera->yaw) * cosf(camera->pitch),
        sinf(camera->pitch),
        cosf(camera->yaw) * cosf(camera->pitch)
    ));

    Vec3 right = cross(front, V3(0.0f, 1.0f, 0.0f));
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
    Mat4 view = Mat4LookAt(camera->pos, camera->pos + front, up);

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

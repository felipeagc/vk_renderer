#pragma once

#include <stdalign.h>
#include "math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Engine Engine;

typedef struct alignas(16) CameraUniform
{
    Vec4 pos;
    Mat4 view;
    Mat4 proj;
} CameraUniform;

typedef struct FPSCamera
{
    Engine *engine;

    Vec3 pos;
    float yaw;
    float pitch;
    float fovy;

    double prev_x;
    double prev_y;
    float sensitivity;
    float speed;
} FPSCamera;

void FPSCameraInit(FPSCamera *camera, Engine *engine);
CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time);

#ifdef __cplusplus
}
#endif

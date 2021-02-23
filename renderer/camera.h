#pragma once

#include "math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Engine Engine;

struct CameraUniform
{
    Vec4 pos;
    Mat4 view;
    Mat4 proj;
};

struct FPSCamera
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
};

void FPSCameraInit(FPSCamera *camera, Engine *engine);
CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time);

#ifdef __cplusplus
}
#endif

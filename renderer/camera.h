#pragma once

#include "math.h"
#include "platform.h"

struct CameraUniform
{
    Vec4 pos;
    Mat4 view;
    Mat4 proj;
};

struct FPSCamera
{
    Platform *platform;

    Vec3 pos;
    float yaw;
    float pitch;
    float fovy;

    double prev_x;
    double prev_y;
    float sensitivity;
    float speed;
};

void FPSCameraInit(FPSCamera *camera, Platform *platform);
CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time);

#pragma once

#include "math.hpp"
#include "platform.hpp"

struct CameraUniform
{
    Vec4 pos;
    Mat4 view;
    Mat4 proj;
};

struct FPSCamera
{
    Platform *platform = nullptr;

    Vec3 pos = Vec3(0.0f);
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fovy = radians(75.0f);

    int32_t prev_x = 0;
    int32_t prev_y = 0;
    float sensitivity = 0.7f;
    float speed = 1.0f;
};

void FPSCameraInit(FPSCamera *camera, Platform *platform);
CameraUniform FPSCameraUpdate(FPSCamera *camera, float delta_time);

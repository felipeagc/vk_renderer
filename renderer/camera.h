#pragma once

#include "base.h"
#include "math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgEngine EgEngine;

typedef struct EgCameraUniform
{
    float4 pos;
    float4x4 view;
    float4x4 proj;
} EgCameraUniform;

typedef struct EgFPSCamera
{
    EgEngine *engine;

    float3 pos;
    float yaw;
    float pitch;
    float fovy;

    double prev_x;
    double prev_y;
    float sensitivity;
    float speed;
} EgFPSCamera;

void egFPSCameraInit(EgFPSCamera *camera, EgEngine *engine);
EgCameraUniform egFPSCameraUpdate(EgFPSCamera *camera, float delta_time);

#ifdef __cplusplus
}
#endif

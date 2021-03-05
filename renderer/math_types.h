#pragma once

#include "base.h"

typedef struct float2
{
    float x, y;
} float2;

typedef struct float3
{
    float x, y, z;
} float3;

typedef struct float4
{
    alignas(16) float x;
    float y;
    float z;
    float w;
} float4;

typedef struct  float4x4
{
    alignas(16) float xx;
    float     xy, xz, xw;
    float yx, yy, yz, yw;
    float zx, zy, zz, zw;
    float wx, wy, wz, ww;
} float4x4;

typedef struct quat128
{
    alignas(16) float x;
    float y;
    float z;
    float w;
} quat128;

EG_STATIC_ASSERT(sizeof(float4) == 16, "wrong float4 size");
EG_STATIC_ASSERT(sizeof(float4x4) == 64, "wrong float4x4 size");
EG_STATIC_ASSERT(sizeof(quat128) == 16, "wrong quat128 size");

EG_STATIC_ASSERT(alignof(float4) == 16, "wrong float4 alignment");
EG_STATIC_ASSERT(alignof(float4x4) == 16, "wrong float4x4 alignment");
EG_STATIC_ASSERT(alignof(quat128) == 16, "wrong quat128 alignment");

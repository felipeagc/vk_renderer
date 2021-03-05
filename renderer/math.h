#pragma once

#include <math.h>
#include <string.h>
#include "base.h"
#include "math_types.h"

#define EG_PI 3.14159265358979323846f

#define EG_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define EG_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define EG_CLAMP(x, lo, hi) (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))
#define EG_LERP(v1, v2, t) ((1 - (t)) * (v1) + (t) * (v2))

#define EG_RADIANS(deg) (deg * (EG_PI / 180.0f))
#define EG_DEGREES(radians) (radians * (180.0f / PI))

EG_INLINE
static float2 V2(float x, float y)
{
    float2 v;
    v.x = x;
    v.y = y;
    return v;
}

EG_INLINE
static float3 V3(float x, float y, float z)
{
    float3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

EG_INLINE
static float4 V4(float x, float y, float z, float w)
{
    float4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

EG_INLINE
static float4x4 eg_float4x4_diagonal(float v)
{
    float4x4 mat;
    memset(&mat, 0, sizeof(mat));

    mat.xx = v;
    mat.yy = v;
    mat.zz = v;
    mat.ww = v;
    return mat;
}

/////////////////////////////
//
// float3 functions
//
/////////////////////////////

EG_INLINE
static float eg_float3_length(float3 vec)
{
    return sqrtf((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
}

EG_INLINE
static float3 eg_float3_add(float3 left, float3 right)
{
    return V3(left.x + right.x, left.y + right.y, left.z + right.z);
}

EG_INLINE
static float3 eg_float3_add_scalar(float3 left, float right)
{
    return V3(left.x + right, left.y + right, left.z + right);
}

EG_INLINE
static float3 eg_float3_sub(float3 left, float3 right)
{
    return V3(left.x - right.x, left.y - right.y, left.z - right.z);
}

EG_INLINE
static float3 eg_float3_sub_scalar(float3 left, float right)
{
    return V3(left.x - right, left.y - right, left.z - right);
}

EG_INLINE
static float3 eg_float3_mul(float3 left, float3 right)
{
    return V3(left.x * right.x, left.y * right.y, left.z * right.z);
}

EG_INLINE
static float3 eg_float3_mul_scalar(float3 left, float right)
{
    return V3(left.x * right, left.y * right, left.z * right);
}

EG_INLINE
static float3 eg_float3_div(float3 left, float3 right)
{
    return V3(left.x / right.x, left.y / right.y, left.z / right.z);
}

EG_INLINE
static float3 eg_float3_div_scalar(float3 left, float right)
{
    return V3(left.x / right, left.y / right, left.z / right);
}

EG_INLINE
static float eg_float3_distance(float3 left, float3 right)
{
    return eg_float3_length(eg_float3_sub(left, right));
}

EG_INLINE
static float3 eg_float3_normalize(float3 vec)
{
    float3 result = vec;
    float norm = eg_float3_length(vec);
    if (norm != 0.0f)
    {
        result = eg_float3_mul_scalar(vec, (1.0f / norm));
    }
    return result;
}

EG_INLINE
static float eg_float3_dot(float3 left, float3 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

EG_INLINE
static float3 eg_float3_cross(float3 left, float3 right)
{
    float3 result;
    result.x = (left.y * right.z) - (left.z * right.y);
    result.y = (left.z * right.x) - (left.x * right.z);
    result.z = (left.x * right.y) - (left.y * right.x);
    return result;
}

/////////////////////////////
//
// float4 functions
//
/////////////////////////////

EG_INLINE
static float4 eg_float4_add(float4 left, float4 right)
{
    return V4(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
}

EG_INLINE
static float4 eg_float4_add_scalar(float4 left, float right)
{
    return V4(left.x + right, left.y + right, left.z + right, left.w + right);
}

EG_INLINE
static float4 eg_float4_sub(float4 left, float4 right)
{
    return V4(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
}

EG_INLINE
static float4 eg_float4_sub_scalar(float4 left, float right)
{
    return V4(left.x - right, left.y - right, left.z - right, left.w - right);
}

EG_INLINE
static float4 eg_float4_mul(float4 left, float4 right)
{
    return V4(left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w);
}

EG_INLINE
static float4 eg_float4_mul_scalar(float4 left, float right)
{
    return V4(left.x * right, left.y * right, left.z * right, left.w * right);
}

EG_INLINE
static float4 eg_float4_div(float4 left, float4 right)
{
    return V4(left.x / right.x, left.y / right.y, left.z / right.z, left.w / right.w);
}

EG_INLINE
static float4 eg_float4_div_scalar(float4 left, float right)
{
    return V4(left.x / right, left.y / right, left.z / right, left.w / right);
}

EG_INLINE
static float eg_float4_dot(float4 left, float4 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z) +
           (left.w * right.w);
}

/////////////////////////////
//
// float4x4 functions
//
/////////////////////////////

EG_INLINE
static float4x4 eg_float4x4_mul_scalar(const float4x4 *left, float right)
{
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float *res = &result.xx;
    const float *l = &left->xx;

    for (unsigned char i = 0; i < 16; i++)
    {
        res[i] += l[i] * right;
    }

    return result;
}

EG_INLINE
static float4x4 eg_float4x4_div_scalar(const float4x4 *left, float right)
{
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float *res = &result.xx;
    const float *l = &left->xx;

    for (unsigned char i = 0; i < 16; i++)
    {
        res[i] += l[i] / right;
    }

    return result;
}

EG_INLINE
static float4x4 eg_float4x4_mul(const float4x4 *left, const float4x4 *right)
{
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float *res = &result.xx;
    const float *l = &left->xx;
    const float *r = &right->xx;

    for (unsigned char i = 0; i < 4; i++)
    {
        for (unsigned char j = 0; j < 4; j++)
        {
            for (unsigned char p = 0; p < 4; p++)
            {
                res[i * 4 + j] += l[i * 4 + p] * r[p * 4 + j];
            }
        }
    }
    return result;
}

EG_INLINE
static float4 eg_float4x4_mul_vector(const float4x4 *left, const float4 *right)
{
    float4 result;

    result.x = left->xx * right->x + left->yx * right->y + left->zx * right->z +
               left->wx * right->w;
    result.y = left->xy * right->x + left->yy * right->y + left->zy * right->z +
               left->wy * right->w;
    result.z = left->xz * right->x + left->yz * right->y + left->zz * right->z +
               left->wz * right->w;
    result.w = left->xw * right->x + left->yw * right->y + left->zw * right->z +
               left->ww * right->w;

    return result;
}

EG_INLINE
static float4x4 eg_float4x4_add(const float4x4 *left, const float4x4 *right)
{
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float *res = &result.xx;
    const float *l = &left->xx;
    const float *r = &right->xx;

    for (unsigned char i = 0; i < 16; i++)
    {
        res[i] += l[i] + r[i];
    }

    return result;
}

EG_INLINE
static float4x4 eg_float4x4_sub(const float4x4 *left, const float4x4 *right)
{
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float *res = &result.xx;
    const float *l = &left->xx;
    const float *r = &right->xx;

    for (unsigned char i = 0; i < 16; i++)
    {
        res[i] += l[i] - r[i];
    }

    return result;
}

static inline float4x4 eg_float4x4_transpose(const float4x4 *mat)
{
    float4x4 result = *mat;
    result.xy = mat->yx;
    result.xz = mat->zx;
    result.xw = mat->wx;

    result.yx = mat->xy;
    result.yz = mat->zy;
    result.yw = mat->wy;

    result.zx = mat->xz;
    result.zy = mat->yz;
    result.zw = mat->wz;

    result.wx = mat->xw;
    result.wy = mat->yw;
    result.wz = mat->zw;
    return result;
}

static inline float4x4 eg_float4x4_inverse(const float4x4 *mat)
{
    float4x4 inv;
    memset(&inv, 0, sizeof(inv));

    float t[6];
    float a = mat->xx, b = mat->xy, c = mat->xz, d = mat->xw, e = mat->yx, f = mat->yy,
          g = mat->yz, h = mat->yw, i = mat->zx, j = mat->zy, k = mat->zz, l = mat->zw,
          m = mat->wx, n = mat->wy, o = mat->wz, p = mat->ww;

    t[0] = k * p - o * l;
    t[1] = j * p - n * l;
    t[2] = j * o - n * k;
    t[3] = i * p - m * l;
    t[4] = i * o - m * k;
    t[5] = i * n - m * j;

    inv.xx = f * t[0] - g * t[1] + h * t[2];
    inv.yx = -(e * t[0] - g * t[3] + h * t[4]);
    inv.zx = e * t[1] - f * t[3] + h * t[5];
    inv.wx = -(e * t[2] - f * t[4] + g * t[5]);

    inv.xy = -(b * t[0] - c * t[1] + d * t[2]);
    inv.yy = a * t[0] - c * t[3] + d * t[4];
    inv.zy = -(a * t[1] - b * t[3] + d * t[5]);
    inv.wy = a * t[2] - b * t[4] + c * t[5];

    t[0] = g * p - o * h;
    t[1] = f * p - n * h;
    t[2] = f * o - n * g;
    t[3] = e * p - m * h;
    t[4] = e * o - m * g;
    t[5] = e * n - m * f;

    inv.xz = b * t[0] - c * t[1] + d * t[2];
    inv.yz = -(a * t[0] - c * t[3] + d * t[4]);
    inv.zz = a * t[1] - b * t[3] + d * t[5];
    inv.wz = -(a * t[2] - b * t[4] + c * t[5]);

    t[0] = g * l - k * h;
    t[1] = f * l - j * h;
    t[2] = f * k - j * g;
    t[3] = e * l - i * h;
    t[4] = e * k - i * g;
    t[5] = e * j - i * f;

    inv.xw = -(b * t[0] - c * t[1] + d * t[2]);
    inv.yw = a * t[0] - c * t[3] + d * t[4];
    inv.zw = -(a * t[1] - b * t[3] + d * t[5]);
    inv.ww = a * t[2] - b * t[4] + c * t[5];

    float det = a * inv.xx + b * inv.yx + c * inv.zx + d * inv.wx;

    inv = eg_float4x4_mul_scalar(&inv, 1.0f / det);

    return inv;
}

static inline float4x4 eg_float4x4_perspective(float fovy, float aspect, float n, float f)
{
    float c = 1.0f / tanf(fovy / 2.0f);

    float4x4 result;
    memset(&result, 0, sizeof(result));

    float4 *cols = (float4 *)&result;

    cols[0] = V4(c / aspect, 0.0f, 0.0f, 0.0f);
    cols[1] = V4(0.0f, c, 0.0f, 0.0f);
    cols[2] = V4(0.0f, 0.0f, -(f + n) / (f - n), -1.0f);
    cols[3] = V4(0.0f, 0.0f, -(2.0f * f * n) / (f - n), 0.0f);

    return result;
}

static inline float4x4
eg_float4x4_perspective_reverse_z(float fovy, float aspect_ratio, float z_near)
{
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float4 *cols = (float4 *)&result;

    float t = tanf(fovy / 2.0f);
    float sy = 1.0f / t;
    float sx = sy / aspect_ratio;

    cols[0] = V4(sx, 0.0f, 0.0f, 0.0f);
    cols[1] = V4(0.0f, sy, 0.0f, 0.0f);
    cols[2] = V4(0.0f, 0.0f, 0.0f, -1.0f);
    cols[3] = V4(0.0f, 0.0f, z_near, 0.0f);

    return result;
}

static inline float4x4 eg_float4x4_look_at(float3 eye, float3 center, float3 up)
{
    float3 f = eg_float3_normalize(eg_float3_sub(center, eye));
    float3 s = eg_float3_normalize(eg_float3_cross(f, up));
    float3 u = eg_float3_cross(s, f);

    float4x4 result = eg_float4x4_diagonal(1.0f);

    result.xx = s.x;
    result.yx = s.y;
    result.zx = s.z;

    result.xy = u.x;
    result.yy = u.y;
    result.zy = u.z;

    result.xz = -f.x;
    result.yz = -f.y;
    result.zz = -f.z;

    result.wx = -eg_float3_dot(s, eye);
    result.wy = -eg_float3_dot(u, eye);
    result.wz = eg_float3_dot(f, eye);

    return result;
}

EG_INLINE static void eg_float4x4_translate(float4x4 *mat, float3 translation)
{
    mat->wx += translation.x;
    mat->wy += translation.y;
    mat->wz += translation.z;
}

EG_INLINE static void eg_float4x4_scale(float4x4 *mat, float3 scale)
{
    mat->xx *= scale.x;
    mat->yy *= scale.y;
    mat->zz *= scale.z;
}

static inline void eg_float4x4_rotate(float4x4 *mat, float angle, float3 axis)
{
    float c = cosf(angle);
    float s = sinf(angle);

    axis = eg_float3_normalize(axis);
    float3 temp = eg_float3_mul_scalar(axis, 1.0f - c);

    float4x4 rotate;
    memset(&rotate, 0, sizeof(rotate));

    rotate.xx = c + temp.x * axis.x;
    rotate.xy = temp.x * axis.y + s * axis.z;
    rotate.xz = temp.x * axis.z - s * axis.y;

    rotate.yx = temp.y * axis.x - s * axis.z;
    rotate.yy = c + temp.y * axis.y;
    rotate.yz = temp.y * axis.z + s * axis.x;

    rotate.zx = temp.z * axis.x + s * axis.y;
    rotate.zy = temp.z * axis.y - s * axis.x;
    rotate.zz = c + temp.z * axis.z;

    float4 *mat_cols = (float4 *)mat;

    float4x4 result;
    memset(&result, 0, sizeof(result));

    float4 *cols = (float4 *)&result;

    cols[0] = eg_float4_mul_scalar(mat_cols[0], rotate.xx);
    cols[0] = eg_float4_add(cols[0], eg_float4_mul_scalar(mat_cols[1], rotate.xy));
    cols[0] = eg_float4_add(cols[0], eg_float4_mul_scalar(mat_cols[2], rotate.xz));

    cols[1] = eg_float4_mul_scalar(mat_cols[0], rotate.yx);
    cols[1] = eg_float4_add(cols[1], eg_float4_mul_scalar(mat_cols[1], rotate.yy));
    cols[1] = eg_float4_add(cols[1], eg_float4_mul_scalar(mat_cols[2], rotate.yz));

    cols[2] = eg_float4_mul_scalar(mat_cols[0], rotate.zx);
    cols[2] = eg_float4_add(cols[2], eg_float4_mul_scalar(mat_cols[1], rotate.zy));
    cols[2] = eg_float4_add(cols[2], eg_float4_mul_scalar(mat_cols[2], rotate.zz));

    cols[3] = mat_cols[3];

    *mat = result;
}

/////////////////////////////
//
// Quaternion functions
//
/////////////////////////////

EG_INLINE
static float eg_quat_dot(quat128 left, quat128 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z) +
           (left.w * right.w);
}

EG_INLINE
static quat128 eg_quat_normalize(quat128 left)
{
    float length = sqrtf(eg_quat_dot(left, left));
    quat128 result;
    if (length <= 0.0f)
    {
        result.x = 0.0f;
        result.y = 0.0f;
        result.z = 0.0f;
        result.w = 0.0f;
        return result;
    }
    float one_over_length = 1.0f / length;
    result.x = left.x * one_over_length;
    result.y = left.y * one_over_length;
    result.z = left.z * one_over_length;
    result.w = left.w * one_over_length;
    return result;
}

EG_INLINE
static quat128 eg_quat_conjugate(quat128 quat)
{
    quat128 result;
    result.w = quat.w;
    result.x = -quat.x;
    result.y = -quat.y;
    result.z = -quat.z;
    return result;
}

static inline quat128 eg_quat_look_at(float3 direction, float3 up)
{
    float m[3][3] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
    };

    float3 col2 = eg_float3_mul_scalar(direction, -1.0f);
    m[2][0] = col2.x;
    m[2][1] = col2.y;
    m[2][2] = col2.z;

    float3 col0 = eg_float3_normalize(eg_float3_cross(up, col2));
    m[0][0] = col0.x;
    m[0][1] = col0.y;
    m[0][2] = col0.z;

    float3 col1 = eg_float3_cross(col2, col0);
    m[1][0] = col1.x;
    m[1][1] = col1.y;
    m[1][2] = col1.z;

    float x = m[0][0] - m[1][1] - m[2][2];
    float y = m[1][1] - m[0][0] - m[2][2];
    float z = m[2][2] - m[0][0] - m[1][1];
    float w = m[0][0] + m[1][1] + m[2][2];

    int biggest_index = 0;
    float biggest = w;
    if (x > biggest)
    {
        biggest = x;
        biggest_index = 1;
    }
    if (y > biggest)
    {
        biggest = y;
        biggest_index = 2;
    }
    if (z > biggest)
    {
        biggest = z;
        biggest_index = 3;
    }

    float biggest_val = sqrtf(biggest + 1.0f) * 0.5f;
    float mult = 0.25f / biggest_val;

    quat128 result;

    switch (biggest_index)
    {
    case 0:
        result.x = (m[1][2] - m[2][1]) * mult;
        result.y = (m[2][0] - m[0][2]) * mult;
        result.z = (m[0][1] - m[1][0]) * mult;
        result.w = biggest_val;
        break;
    case 1:
        result.x = biggest_val;
        result.y = (m[0][1] + m[1][0]) * mult;
        result.z = (m[2][0] + m[0][2]) * mult;
        result.w = (m[1][2] - m[2][1]) * mult;
        break;
    case 2:
        result.x = (m[0][1] + m[1][0]) * mult;
        result.y = biggest_val;
        result.z = (m[1][2] + m[2][1]) * mult;
        result.w = (m[2][0] - m[0][2]) * mult;
        break;
    case 3:
        result.x = (m[2][0] + m[0][2]) * mult;
        result.y = (m[1][2] + m[2][1]) * mult;
        result.z = biggest_val;
        result.w = (m[0][1] - m[1][0]) * mult;
        break;
    default: EG_ASSERT(0); break;
    }

    return result;
}

EG_INLINE
static quat128 eg_quat_from_axis_angle(float3 axis, float angle)
{
    float s = sinf(angle / 2.0f);
    quat128 result;
    result.x = axis.x * s;
    result.y = axis.y * s;
    result.z = axis.z * s;
    result.w = cosf(angle / 2.0f);
    return result;
}

static inline quat128 eg_quat_from_matrix(const float4x4 *mat)
{
    quat128 result;
    float trace = mat->xx + mat->yy + mat->zz;
    if (trace > 0.0f)
    {
        float s = sqrtf(1.0f + trace) * 2.0f;
        result.w = 0.25f * s;
        result.x = (mat->yz - mat->zy) / s;
        result.y = (mat->zx - mat->xz) / s;
        result.z = (mat->xy - mat->yx) / s;
    }
    else if (mat->xx > mat->yy && mat->xx > mat->zz)
    {
        float s = sqrtf(1.0f + mat->xx - mat->yy - mat->zz) * 2.0f;
        result.w = (mat->yz - mat->zy) / s;
        result.x = 0.25f * s;
        result.y = (mat->yx + mat->xy) / s;
        result.z = (mat->zx + mat->xz) / s;
    }
    else if (mat->yy > mat->zz)
    {
        float s = sqrtf(1.0f + mat->yy - mat->xx - mat->zz) * 2.0f;
        result.w = (mat->zx - mat->xz) / s;
        result.x = (mat->yx + mat->xy) / s;
        result.y = 0.25f * s;
        result.z = (mat->zy + mat->yz) / s;
    }
    else
    {
        float s = sqrtf(1.0f + mat->zz - mat->xx - mat->yy) * 2.0f;
        result.w = (mat->xy - mat->yx) / s;
        result.x = (mat->zx + mat->xz) / s;
        result.y = (mat->zy + mat->yz) / s;
        result.z = 0.25f * s;
    }
    return result;
}

static inline void eg_quat_to_axis_angle(quat128 quat, float3 *axis, float *angle)
{
    quat = eg_quat_normalize(quat);
    *angle = 2.0f * acosf(quat.w);
    float s = sqrtf(1.0f - quat.w * quat.w);
    if (s < 0.001)
    {
        axis->x = quat.x;
        axis->y = quat.y;
        axis->z = quat.z;
    }
    else
    {
        axis->x = quat.x / s;
        axis->y = quat.y / s;
        axis->z = quat.z / s;
    }
}

static inline float4x4 eg_quat_to_matrix(quat128 quat)
{
    float4x4 result = eg_float4x4_diagonal(1.0f);

    float xx = quat.x * quat.x;
    float yy = quat.y * quat.y;
    float zz = quat.z * quat.z;
    float xy = quat.x * quat.y;
    float xz = quat.x * quat.z;
    float yz = quat.y * quat.z;
    float wx = quat.w * quat.x;
    float wy = quat.w * quat.y;
    float wz = quat.w * quat.z;

    result.xx = 1.0f - 2.0f * (yy + zz);
    result.xy = 2.0f * (xy + wz);
    result.xz = 2.0f * (xz - wy);

    result.yx = 2.0f * (xy - wz);
    result.yy = 1.0f - 2.0f * (xx + zz);
    result.yz = 2.0f * (yz + wx);

    result.zx = 2.0f * (xz + wy);
    result.zy = 2.0f * (yz - wx);
    result.zz = 1.0f - 2.0f * (xx + yy);

    return result;
}

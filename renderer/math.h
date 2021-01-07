#pragma once

#include <assert.h>
#include <math.h>
#include <string.h>

#if defined(_MSC_VER)
#define MATH_INLINE __forceinline inline
#elif defined(__clang__) || defined(__GNUC__)
#define MATH_INLINE __attribute__((always_inline)) inline
#else
#define MATH_INLINE inline
#endif

constexpr float PI = 3.14159265358979323846f;

template<typename T>
MATH_INLINE
T max(T a, T b)
{
    return (a > b) ? a : b;
}

template<typename T>
MATH_INLINE
T min(T a, T b)
{
    return (a < b) ? a : b;
}

template<typename T>
MATH_INLINE
T clamp(T x, T lo, T hi)
{
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

template<typename T>
MATH_INLINE
T lerp(T v1, T v2, T t)
{
    return ((1 - t) * v1 + t * v2);
}

template<typename T>
MATH_INLINE
T radians(T deg)
{
    return deg * (PI / 180.0f);
}

template<typename T>
MATH_INLINE
T degrees(T radians)
{
    return radians * (180.0f / PI);
}

/////////////////////////////
//
// Types
// 
/////////////////////////////

union Vec2
{
    struct { float x, y; };
    struct { float r, g; };
    float v[2];

    MATH_INLINE
    Vec2()
    {
        this->x = 0.0f;
        this->y = 0.0f;
    }

    MATH_INLINE
    Vec2(float v)
    {
        this->x = v;
        this->y = v;
    }

    MATH_INLINE
    Vec2(float x, float y)
    {
        this->x = x;
        this->y = y;
    }

    float &operator[](int index)
    {
        return this->v[index];
    }
};

union Vec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    float v[3];

    MATH_INLINE
    Vec3()
    {
        this->x = 0.0f;
        this->y = 0.0f;
        this->z = 0.0f;
    }

    MATH_INLINE
    Vec3(float v)
    {
        this->x = v;
        this->y = v;
        this->z = v;
    }

    MATH_INLINE
    Vec3(float x, float y, float z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    float &operator[](int index)
    {
        return this->v[index];
    }
};

union Vec4
{
    struct {
        union {
            struct { float x, y, z; };
        };
        float w;
    };

    struct {
        union {
            struct { float r, g, b; };
        };
        float a;
    };

    float v[4];

    MATH_INLINE
    Vec4()
    {
        this->x = 0.0f;
        this->y = 0.0f;
        this->z = 0.0f;
        this->w = 0.0f;
    }

    MATH_INLINE
    Vec4(float v)
    {
        this->x = v;
        this->y = v;
        this->z = v;
        this->w = v;
    }

    MATH_INLINE
    Vec4(float x, float y, float z, float w)
    {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }

    float &operator[](int index)
    {
        return this->v[index];
    }
};

union Mat4
{
    float cols[4][4];
    float elems[16];
    Vec4 v[4];

    Vec4 &operator[](int index)
    {
        return this->v[index];
    }
};

struct Quat
{
    float x, y, z, w;
};

MATH_INLINE
static Vec2 V2(float x, float y)
{
    Vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

MATH_INLINE
static Vec3 V3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

MATH_INLINE
static Vec4 V4(float x, float y, float z, float w)
{
    Vec4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

MATH_INLINE
static Mat4 Mat4Diagonal(float v)
{
    Mat4 mat = {};
    mat.cols[0][0] = v;
    mat.cols[1][1] = v;
    mat.cols[2][2] = v;
    mat.cols[3][3] = v;
    return mat;
}

/////////////////////////////
//
// Vec3 functions
// 
/////////////////////////////

MATH_INLINE
static float length(Vec3 vec)
{
    return sqrtf((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
}

MATH_INLINE
static Vec3 operator+(Vec3 left, Vec3 right)
{
    return V3(
        left.x + right.x,
        left.y + right.y,
        left.z + right.z
    );
}

MATH_INLINE
static Vec3 operator+(Vec3 left, float right)
{
    return V3(
        left.x + right,
        left.y + right,
        left.z + right
    );
}

MATH_INLINE
static Vec3 operator-(Vec3 left, Vec3 right)
{
    return V3(
        left.x - right.x,
        left.y - right.y,
        left.z - right.z
    );
}

MATH_INLINE
static Vec3 operator-(Vec3 left, float right)
{
    return V3(
        left.x - right,
        left.y - right,
        left.z - right
    );
}

MATH_INLINE
static Vec3 operator*(Vec3 left, Vec3 right)
{
    return V3(
        left.x * right.x,
        left.y * right.y,
        left.z * right.z
    );
}

MATH_INLINE
static Vec3 operator*(Vec3 left, float right)
{
    return V3(
        left.x * right,
        left.y * right,
        left.z * right
    );
}

MATH_INLINE
static Vec3 operator/(Vec3 left, Vec3 right)
{
    return V3(
        left.x / right.x,
        left.y / right.y,
        left.z / right.z
    );
}

MATH_INLINE
static Vec3 operator/(Vec3 left, float right)
{
    return V3(
        left.x / right,
        left.y / right,
        left.z / right
    );
}

MATH_INLINE
static float distance(Vec3 left, Vec3 right)
{
    return length(left - right);
}

MATH_INLINE
static float dot(Vec3 left, Vec3 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

MATH_INLINE
static Vec3 cross(Vec3 left, Vec3 right)
{
    Vec3 result;
    result.x = (left.y * right.z) - (left.z * right.y);
    result.y = (left.z * right.x) - (left.x * right.z);
    result.z = (left.x * right.y) - (left.y * right.x);
    return result;
}

MATH_INLINE
static Vec3 normalize(Vec3 vec)
{
    Vec3 result = vec;
    float norm = length(vec);
    if (norm != 0.0f) {
        result = vec * (1.0f / norm);
    }
    return result;
}

/////////////////////////////
//
// Vec4 functions
// 
/////////////////////////////

MATH_INLINE
static Vec4 operator+(Vec4 left, Vec4 right)
{
    return V4(
        left.x + right.x,
        left.y + right.y,
        left.z + right.z,
        left.w + right.w
    );
}

MATH_INLINE
static Vec4 operator+(Vec4 left, float right)
{
    return V4(
        left.x + right,
        left.y + right,
        left.z + right,
        left.w + right
    );
}

MATH_INLINE
static Vec4 operator-(Vec4 left, Vec4 right)
{
    return V4(
        left.x - right.x,
        left.y - right.y,
        left.z - right.z,
        left.w - right.w
    );
}

MATH_INLINE
static Vec4 operator-(Vec4 left, float right)
{
    return V4(
        left.x - right,
        left.y - right,
        left.z - right,
        left.w - right
    );
}

MATH_INLINE
static Vec4 operator*(Vec4 left, Vec4 right)
{
    return V4(
        left.x * right.x,
        left.y * right.y,
        left.z * right.z,
        left.w * right.w
    );
}

MATH_INLINE
static Vec4 operator*(Vec4 left, float right)
{
    return V4(
        left.x * right,
        left.y * right,
        left.z * right,
        left.w * right
    );
}

MATH_INLINE
static Vec4 operator/(Vec4 left, Vec4 right)
{
    return V4(
        left.x / right.x,
        left.y / right.y,
        left.z / right.z,
        left.w / right.w
    );
}

MATH_INLINE
static Vec4 operator/(Vec4 left, float right)
{
    return V4(
        left.x / right,
        left.y / right,
        left.z / right,
        left.w / right
    );
}

MATH_INLINE
static float dot(Vec4 left, Vec4 right) 
{
    return (left.x * right.x) +
        (left.y * right.y) +
        (left.z * right.z) +
        (left.w * right.w);
}

/////////////////////////////
//
// Mat4 functions
// 
/////////////////////////////

MATH_INLINE
static Mat4 operator+(const Mat4 &left, const Mat4 &right)
{
    Mat4 result = {};
    for (unsigned char i = 0; i < 16; i++) {
        result.elems[i] = left.elems[i] + right.elems[i];
    }
    return result;
}

MATH_INLINE
static Mat4 operator-(const Mat4 &left, const Mat4 &right)
{
    Mat4 result = {};
    for (unsigned char i = 0; i < 16; i++) {
        result.elems[i] = left.elems[i] - right.elems[i];
    }
    return result;
}

MATH_INLINE
static Mat4 operator*(const Mat4 &left, float right)
{
    Mat4 result = {};
    for (unsigned char i = 0; i < 16; i++) {
        result.elems[i] = left.elems[i] * right;
    }
    return result;
}

MATH_INLINE
static Mat4 operator/(const Mat4 &left, float right)
{
    Mat4 result = {};
    for (unsigned char i = 0; i < 16; i++) {
        result.elems[i] = left.elems[i] / right;
    }
    return result;
}

MATH_INLINE
static Mat4 operator*(const Mat4 &left, const Mat4 &right)
{
    Mat4 result = {};
    for (unsigned char i = 0; i < 4; i++) {
        for (unsigned char j = 0; j < 4; j++) {
            for (unsigned char p = 0; p < 4; p++) {
                result.cols[i][j] += left.cols[i][p] * right.cols[p][j];
            }
        }
    }
    return result;
}

static inline
Vec4 operator*(const Mat4 &left, const Vec4 &right)
{
    Vec4 result;

    result.v[0] =
        left.cols[0][0] * right.v[0] +
        left.cols[1][0] * right.v[1] +
        left.cols[2][0] * right.v[2] +
        left.cols[3][0] * right.v[3];
    result.v[1] =
        left.cols[0][1] * right.v[0] +
        left.cols[1][1] * right.v[1] +
        left.cols[2][1] * right.v[2] +
        left.cols[3][1] * right.v[3];
    result.v[2] =
        left.cols[0][2] * right.v[0] +
        left.cols[1][2] * right.v[1] +
        left.cols[2][2] * right.v[2] +
        left.cols[3][2] * right.v[3];
    result.v[3] =
        left.cols[0][3] * right.v[0] +
        left.cols[1][3] * right.v[1] +
        left.cols[2][3] * right.v[2] +
        left.cols[3][3] * right.v[3];

    return result;
}

static inline
Mat4 Mat4Transpose(const Mat4 &mat)
{
    Mat4 result = mat;
    result.cols[0][1] = mat.cols[1][0];
    result.cols[0][2] = mat.cols[2][0];
    result.cols[0][3] = mat.cols[3][0];

    result.cols[1][0] = mat.cols[0][1];
    result.cols[1][2] = mat.cols[2][1];
    result.cols[1][3] = mat.cols[3][1];

    result.cols[2][0] = mat.cols[0][2];
    result.cols[2][1] = mat.cols[1][2];
    result.cols[2][3] = mat.cols[3][2];

    result.cols[3][0] = mat.cols[0][3];
    result.cols[3][1] = mat.cols[1][3];
    result.cols[3][2] = mat.cols[2][3];
    return result;
}

static inline
Mat4 Mat4Inverse(const Mat4 &mat)
{
    Mat4 inv = {};

    float t[6];
    float a = mat.cols[0][0], b = mat.cols[0][1], c = mat.cols[0][2],
    d = mat.cols[0][3], e = mat.cols[1][0], f = mat.cols[1][1],
    g = mat.cols[1][2], h = mat.cols[1][3], i = mat.cols[2][0],
    j = mat.cols[2][1], k = mat.cols[2][2], l = mat.cols[2][3],
    m = mat.cols[3][0], n = mat.cols[3][1], o = mat.cols[3][2],
    p = mat.cols[3][3];

    t[0] = k * p - o * l;
    t[1] = j * p - n * l;
    t[2] = j * o - n * k;
    t[3] = i * p - m * l;
    t[4] = i * o - m * k;
    t[5] = i * n - m * j;

    inv.cols[0][0] = f * t[0] - g * t[1] + h * t[2];
    inv.cols[1][0] = -(e * t[0] - g * t[3] + h * t[4]);
    inv.cols[2][0] = e * t[1] - f * t[3] + h * t[5];
    inv.cols[3][0] = -(e * t[2] - f * t[4] + g * t[5]);

    inv.cols[0][1] = -(b * t[0] - c * t[1] + d * t[2]);
    inv.cols[1][1] = a * t[0] - c * t[3] + d * t[4];
    inv.cols[2][1] = -(a * t[1] - b * t[3] + d * t[5]);
    inv.cols[3][1] = a * t[2] - b * t[4] + c * t[5];

    t[0] = g * p - o * h;
    t[1] = f * p - n * h;
    t[2] = f * o - n * g;
    t[3] = e * p - m * h;
    t[4] = e * o - m * g;
    t[5] = e * n - m * f;

    inv.cols[0][2] = b * t[0] - c * t[1] + d * t[2];
    inv.cols[1][2] = -(a * t[0] - c * t[3] + d * t[4]);
    inv.cols[2][2] = a * t[1] - b * t[3] + d * t[5];
    inv.cols[3][2] = -(a * t[2] - b * t[4] + c * t[5]);

    t[0] = g * l - k * h;
    t[1] = f * l - j * h;
    t[2] = f * k - j * g;
    t[3] = e * l - i * h;
    t[4] = e * k - i * g;
    t[5] = e * j - i * f;

    inv.cols[0][3] = -(b * t[0] - c * t[1] + d * t[2]);
    inv.cols[1][3] = a * t[0] - c * t[3] + d * t[4];
    inv.cols[2][3] = -(a * t[1] - b * t[3] + d * t[5]);
    inv.cols[3][3] = a * t[2] - b * t[4] + c * t[5];

    float det = a * inv.cols[0][0] + b * inv.cols[1][0] + c * inv.cols[2][0] +
        d * inv.cols[3][0];

    inv = inv * (1.0f / det);

    return inv;
}

static inline
Mat4 Mat4Perspective(float fovy, float aspect, float n, float f)
{
    float c = 1.0f / tanf(fovy / 2.0f);

    Mat4 result = {};
    result.v[0] = V4(c/aspect, 0.0f,  0.0f,              0.0f);
    result.v[1] = V4(0.0f,     c,     0.0f,              0.0f);
    result.v[2] = V4(0.0f,     0.0f, -(f+n)/(f-n),      -1.0f);
    result.v[3] = V4(0.0f,     0.0f, -(2.0f*f*n)/(f-n),  0.0f);
    return result;
}

static inline
Mat4 Mat4PerspectiveReverseZ(float fovy, float aspect_ratio, float z_near)
{
    Mat4 result = {};

    float t = tanf(fovy / 2.0f);
    float sy = 1.0f / t;
    float sx = sy / aspect_ratio;

    result.v[0] = V4(sx,    0.0f,  0.0f,   0.0f);
    result.v[1] = V4(0.0f,  sy,    0.0f,   0.0f);
    result.v[2] = V4(0.0f,  0.0f,  0.0f,  -1.0f);
    result.v[3] = V4(0.0f,  0.0f,  z_near, 0.0f);

    return result;
}

static inline
Mat4 Mat4LookAt(Vec3 eye, Vec3 center, Vec3 up)
{
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 result = Mat4Diagonal(1.0f);

    result.cols[0][0] = s.x;
    result.cols[1][0] = s.y;
    result.cols[2][0] = s.z;

    result.cols[0][1] = u.x;
    result.cols[1][1] = u.y;
    result.cols[2][1] = u.z;

    result.cols[0][2] = -f.x;
    result.cols[1][2] = -f.y;
    result.cols[2][2] = -f.z;

    result.cols[3][0] = -dot(s, eye);
    result.cols[3][1] = -dot(u, eye);
    result.cols[3][2] = dot(f, eye);

    return result;
}

MATH_INLINE static
void Mat4Translate(Mat4 *mat, Vec3 translation)
{
    mat->cols[3][0] += translation.x;
    mat->cols[3][1] += translation.y;
    mat->cols[3][2] += translation.z;
}

MATH_INLINE static
void Mat4Scale(Mat4 *mat, Vec3 scale)
{
    mat->cols[0][0] *= scale.x;
    mat->cols[1][1] *= scale.y;
    mat->cols[2][2] *= scale.z;
}

static inline
void Mat4Rotate(Mat4 *mat, float angle, Vec3 axis)
{
    float c = cosf(angle);
    float s = sinf(angle);

    axis = normalize(axis);
    Vec3 temp = axis * (1.0f - c);

    Mat4 rotate = {};
    rotate.cols[0][0] = c + temp.x * axis.x;
    rotate.cols[0][1] = temp.x * axis.y + s * axis.z;
    rotate.cols[0][2] = temp.x * axis.z - s * axis.y;

    rotate.cols[1][0] = temp.y * axis.x - s * axis.z;
    rotate.cols[1][1] = c + temp.y * axis.y;
    rotate.cols[1][2] = temp.y * axis.z + s * axis.x;

    rotate.cols[2][0] = temp.z * axis.x + s * axis.y;
    rotate.cols[2][1] = temp.z * axis.y - s * axis.x;
    rotate.cols[2][2] = c + temp.z * axis.z;

    Mat4 result = {};
    result.v[0] =
        (mat->v[0] * rotate.cols[0][0]) +
        (mat->v[1] * rotate.cols[0][1]) +
        (mat->v[2] * rotate.cols[0][2]);
    result.v[1] =
        (mat->v[0] * rotate.cols[1][0]) +
        (mat->v[1] * rotate.cols[1][1]) +
        (mat->v[2] * rotate.cols[1][2]);
    result.v[2] =
        (mat->v[0] * rotate.cols[2][0]) +
        (mat->v[1] * rotate.cols[2][1]) +
        (mat->v[2] * rotate.cols[2][2]);
    result.v[3] = mat->v[3];

    *mat = result;
}

/////////////////////////////
//
// Quaternion functions
// 
/////////////////////////////

MATH_INLINE
static float dot(Quat left, Quat right)
{
    return (left.x * right.x) +
        (left.y * right.y) +
        (left.z * right.z) +
        (left.w * right.w);
}

MATH_INLINE
static Quat normalize(Quat left)
{
    float length = sqrtf(dot(left, left));
    Quat result;
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

MATH_INLINE
static Quat QuatConjugate(Quat quat)
{
    Quat result;
    result.w = quat.w;
    result.x = -quat.x;
    result.y = -quat.y;
    result.z = -quat.z;
    return result;
}

static inline
Quat QuatLookAt(Vec3 direction, Vec3 up)
{
    float m[3][3] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
    };

    Vec3 col2 = direction * -1.0f;
    m[2][0] = col2.x;
    m[2][1] = col2.y;
    m[2][2] = col2.z;

    Vec3 col0 = normalize(cross(up, col2));
    m[0][0] = col0.x;
    m[0][1] = col0.y;
    m[0][2] = col0.z;

    Vec3 col1 = cross(col2, col0);
    m[1][0] = col1.x;
    m[1][1] = col1.y;
    m[1][2] = col1.z;

    float x = m[0][0] - m[1][1] - m[2][2];
    float y = m[1][1] - m[0][0] - m[2][2];
    float z = m[2][2] - m[0][0] - m[1][1];
    float w = m[0][0] + m[1][1] + m[2][2];

    int biggest_index = 0;
    float biggest = w;
    if (x > biggest) {
        biggest = x;
        biggest_index = 1;
    }
    if (y > biggest) {
        biggest = y;
        biggest_index = 2;
    }
    if (z > biggest) {
        biggest = z;
        biggest_index = 3;
    }

    float biggest_val = sqrtf(biggest + 1.0f) * 0.5f;
    float mult = 0.25f / biggest_val;

    Quat result;

    switch (biggest_index) {
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
        default:
            assert(0);
            break;
    }

    return result;
}

MATH_INLINE
static Quat QuatFromAxisAngle(Vec3 axis, float angle)
{
    float s = sinf(angle / 2.0f);
    Quat result;
    result.x = axis.x * s;
    result.y = axis.y * s;
    result.z = axis.z * s;
    result.w = cosf(angle / 2.0f);
    return result;
}

static inline
Quat QuatFromMat4(const Mat4 &mat)
{
    Quat result;
    float trace = mat.cols[0][0] + mat.cols[1][1] + mat.cols[2][2];
    if (trace > 0.0f) {
        float s = sqrtf(1.0f + trace) * 2.0f;
        result.w = 0.25f * s;
        result.x = (mat.cols[1][2] - mat.cols[2][1]) / s;
        result.y = (mat.cols[2][0] - mat.cols[0][2]) / s;
        result.z = (mat.cols[0][1] - mat.cols[1][0]) / s;
    } else if (
            mat.cols[0][0] > mat.cols[1][1] && mat.cols[0][0] > mat.cols[2][2]) {
        float s =
            sqrtf(1.0f + mat.cols[0][0] - mat.cols[1][1] - mat.cols[2][2]) * 2.0f;
        result.w = (mat.cols[1][2] - mat.cols[2][1]) / s;
        result.x = 0.25f * s;
        result.y = (mat.cols[1][0] + mat.cols[0][1]) / s;
        result.z = (mat.cols[2][0] + mat.cols[0][2]) / s;
    } else if (mat.cols[1][1] > mat.cols[2][2]) {
        float s =
            sqrtf(1.0f + mat.cols[1][1] - mat.cols[0][0] - mat.cols[2][2]) * 2.0f;
        result.w = (mat.cols[2][0] - mat.cols[0][2]) / s;
        result.x = (mat.cols[1][0] + mat.cols[0][1]) / s;
        result.y = 0.25f * s;
        result.z = (mat.cols[2][1] + mat.cols[1][2]) / s;
    } else {
        float s =
            sqrtf(1.0f + mat.cols[2][2] - mat.cols[0][0] - mat.cols[1][1]) * 2.0f;
        result.w = (mat.cols[0][1] - mat.cols[1][0]) / s;
        result.x = (mat.cols[2][0] + mat.cols[0][2]) / s;
        result.y = (mat.cols[2][1] + mat.cols[1][2]) / s;
        result.z = 0.25f * s;
    }
    return result;
}

static inline
void QuatToAxisAngle(Quat quat, Vec3 *axis, float *angle)
{
    quat = normalize(quat);
    *angle = 2.0f * acosf(quat.w);
    float s = sqrtf(1.0f - quat.w * quat.w);
    if (s < 0.001) {
        axis->x = quat.x;
        axis->y = quat.y;
        axis->z = quat.z;
    } else {
        axis->x = quat.x / s;
        axis->y = quat.y / s;
        axis->z = quat.z / s;
    }
}

static inline
Mat4 QuatToMat4(Quat quat)
{
    Mat4 result = Mat4Diagonal(1.0f);

    float xx = quat.x * quat.x;
    float yy = quat.y * quat.y;
    float zz = quat.z * quat.z;
    float xy = quat.x * quat.y;
    float xz = quat.x * quat.z;
    float yz = quat.y * quat.z;
    float wx = quat.w * quat.x;
    float wy = quat.w * quat.y;
    float wz = quat.w * quat.z;

    result.cols[0][0] = 1.0f - 2.0f * (yy + zz);
    result.cols[0][1] = 2.0f * (xy + wz);
    result.cols[0][2] = 2.0f * (xz - wy);

    result.cols[1][0] = 2.0f * (xy - wz);
    result.cols[1][1] = 1.0f - 2.0f * (xx + zz);
    result.cols[1][2] = 2.0f * (yz + wx);

    result.cols[2][0] = 2.0f * (xz + wy);
    result.cols[2][1] = 2.0f * (yz - wx);
    result.cols[2][2] = 1.0f - 2.0f * (xx + yy);

    return result;
}

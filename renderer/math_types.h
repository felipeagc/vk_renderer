#pragma once

#if defined(_MSC_VER)
#define MATH_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define MATH_INLINE __attribute__((always_inline)) inline
#else
#define MATH_INLINE inline
#endif

/////////////////////////////
//
// Types
// 
/////////////////////////////

typedef union Vec2 Vec2;
typedef union Vec3 Vec3;
typedef union Vec4 Vec4;
typedef union Mat4 Mat4;
typedef struct Quat Quat;

union Vec2
{
    struct { float x, y; };
    struct { float r, g; };
    float v[2];

#ifdef __cplusplus
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

    MATH_INLINE
	float &operator[](int index)
    {
        return this->v[index];
    }
#endif
};

union Vec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    float v[3];

#ifdef __cplusplus
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

	MATH_INLINE
    float &operator[](int index)
    {
        return this->v[index];
    }
#endif
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

#ifdef __cplusplus
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

	MATH_INLINE
    float &operator[](int index)
    {
        return this->v[index];
    }
#endif
};

union Mat4
{
    float cols[4][4];
    float elems[16];
    Vec4 v[4];

#ifdef __cplusplus
	MATH_INLINE
    Vec4 &operator[](int index)
    {
        return this->v[index];
    }
#endif
};

struct Quat
{
    float x, y, z, w;
};


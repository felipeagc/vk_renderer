#pragma once

#include <stdalign.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define EG_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define EG_INLINE __attribute__((always_inline)) __attribute__((unused)) inline
#else
#define EG_INLINE inline
#endif

#ifdef __GNUC__
    #define EG_PRINTF_FORMATTING(x, y) __attribute__((format(printf, x, y)))
#else
    #define EG_PRINTF_FORMATTING(x, y)
#endif

#define EG_CARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

#define EG_STR(a) #a

#define EG_ASSERT(value)                                                                 \
    do                                                                                   \
    {                                                                                    \
        if (!(value))                                                                    \
        {                                                                                \
            fprintf(                                                                     \
                stderr,                                                                  \
                "Engine assertion failed: '%s' at %s:%d\n",                              \
                EG_STR(value),                                                           \
                __FILE__,                                                                \
                __LINE__);                                                               \
            abort();                                                                     \
        }                                                                                \
    } while (0)

#ifndef __cplusplus
#define EG_STATIC_ASSERT(value, msg) _Static_assert(value, msg)
#else
#define EG_STATIC_ASSERT(value, msg) static_assert(value, msg)
#endif

#ifdef __cplusplus
}
#endif

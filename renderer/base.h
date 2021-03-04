#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

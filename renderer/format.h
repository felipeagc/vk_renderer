#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;

#ifdef __GNUC__
    #define FORMAT_PRINTF_FORMATTING(x, y) __attribute__((format(printf, x, y)))
#else
    #define FORMAT_PRINTF_FORMATTING(x, y)
#endif

FORMAT_PRINTF_FORMATTING(2, 3)
const char* Sprintf(Allocator *allocator, const char *format, ...);

const char* Vsprintf(Allocator *allocator, const char *format, va_list args);

#ifdef __cplusplus
}
#endif

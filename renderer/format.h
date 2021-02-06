#pragma once

typedef struct Allocator Allocator;

#ifdef __GNUC__
    #define PRINTF_FORMATTING(x, y) __attribute__((format(printf, x, y)))
#else
    #define PRINTF_FORMATTING(x, y)
#endif

PRINTF_FORMATTING(2, 3)
const char* Sprintf(Allocator *allocator, const char *format, ...);

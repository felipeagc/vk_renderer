#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
    #define STRING_BUILDER_PRINTF_FORMATTING(x, y) __attribute__((format(printf, x, y)))
#else
    #define STRING_BUILDER_PRINTF_FORMATTING(x, y)
#endif

typedef struct Allocator Allocator;
typedef struct StringBuilder StringBuilder;

StringBuilder *StringBuilderCreate(Allocator *allocator);
void StringBuilderDestroy(StringBuilder *sb);

void StringBuilderAppend(StringBuilder *sb, const char *str);
void StringBuilderAppendLen(StringBuilder *sb, const char *str, size_t length);
STRING_BUILDER_PRINTF_FORMATTING(2, 3)
void StringBuilderAppendFormat(StringBuilder *sb, const char *format, ...);
const char *StringBuilderBuild(StringBuilder *sb, Allocator *allocator);

#ifdef __cplusplus
}
#endif

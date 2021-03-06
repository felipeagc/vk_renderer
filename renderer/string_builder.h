#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;
typedef struct EgStringBuilder EgStringBuilder;

EgStringBuilder *egStringBuilderCreate(EgAllocator *allocator);
void egStringBuilderDestroy(EgStringBuilder *sb);

void egStringBuilderAppend(EgStringBuilder *sb, const char *str);
void egStringBuilderAppendLen(EgStringBuilder *sb, const char *str, size_t length);
EG_PRINTF_FORMATTING(2, 3)
void egStringBuilderAppendFormat(EgStringBuilder *sb, const char *format, ...);
const char *egStringBuilderBuild(EgStringBuilder *sb, EgAllocator *allocator);

#ifdef __cplusplus
}
#endif

#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;

EG_PRINTF_FORMATTING(2, 3)
const char* egSprintf(EgAllocator *allocator, const char *format, ...);

const char* egVsprintf(EgAllocator *allocator, const char *format, va_list args);

#ifdef __cplusplus
}
#endif

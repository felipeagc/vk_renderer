#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;
typedef struct EgPool EgPool;

EgPool *egPoolCreate(EgAllocator *allocator, uint32_t slot_count);
void egPoolDestroy(EgPool *pool);

uint32_t egPoolGetSlotCount(EgPool *pool);
uint32_t egPoolGetFreeSlotCount(EgPool *pool);

uint32_t egPoolAllocateSlot(EgPool *pool);
void egPoolFreeSlot(EgPool *pool, uint32_t slot);

#ifdef __cplusplus
}
#endif

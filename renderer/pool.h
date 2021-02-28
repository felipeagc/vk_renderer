#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Pool Pool;

Pool *PoolCreate(Allocator *allocator, uint32_t slot_count);
void PoolDestroy(Pool *pool);

uint32_t PoolGetSlotCount(Pool *pool);
uint32_t PoolGetFreeSlotCount(Pool *pool);

uint32_t PoolAllocateSlot(Pool *pool);
void PoolFreeSlot(Pool *pool, uint32_t slot);

#ifdef __cplusplus
}
#endif

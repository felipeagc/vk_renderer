#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;
typedef struct EgEngine EgEngine;
typedef struct EgBufferPool EgBufferPool;

EgBufferPool *egBufferPoolCreate(EgAllocator *allocator, EgEngine *engine, size_t item_size, size_t item_count);
void egBufferPoolDestroy(EgBufferPool *pool);

uint32_t egBufferPoolGetBufferIndex(EgBufferPool *pool);

void egBufferPoolReset(EgBufferPool *pool);

// Returns the item index in the buffer
uint32_t egBufferPoolAllocateItem(EgBufferPool *pool, size_t size, void *data);


#ifdef __cplusplus
}
#endif

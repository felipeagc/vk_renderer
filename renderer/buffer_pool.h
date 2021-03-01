#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Engine Engine;
typedef struct BufferPool BufferPool;

BufferPool *BufferPoolCreate(Allocator *allocator, Engine *engine, size_t item_size, size_t item_count);
void BufferPoolDestroy(BufferPool *pool);

uint32_t BufferPoolGetBufferIndex(BufferPool *pool);

void BufferPoolReset(BufferPool *pool);

// Returns the item index in the buffer
uint32_t BufferPoolAllocateItem(BufferPool *pool, size_t size, void *data);


#ifdef __cplusplus
}
#endif

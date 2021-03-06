#include "buffer_pool.h"

#include <string.h>
#include "rg.h"
#include "allocator.h"
#include "engine.h"

struct EgBufferPool
{
    EgAllocator *allocator;
    EgEngine *engine;

	uint32_t frame_index;

	size_t item_size;
	size_t item_count;

	EgBuffer buffer;
	size_t allocated_items;

    uint8_t *mapping;
};

EgBufferPool *egBufferPoolCreate(EgAllocator *allocator, EgEngine *engine, size_t item_size, size_t item_count)
{
	EgBufferPool *pool = egAllocate(allocator, sizeof(*pool));
	memset(pool, 0, sizeof(*pool));

    RgDevice *device = egEngineGetDevice(engine);

	pool->allocator = allocator;
	pool->engine = engine;
	pool->item_size = item_size;
	pool->item_count = item_count;

    RgBufferInfo buffer_info = {};
    buffer_info.size = item_size * item_count * 2;
    buffer_info.usage = RG_BUFFER_USAGE_STORAGE | RG_BUFFER_USAGE_TRANSFER_DST;
    buffer_info.memory = RG_BUFFER_MEMORY_HOST;
    pool->buffer = egEngineAllocateStorageBuffer(engine, &buffer_info);

    pool->mapping = (uint8_t*)rgBufferMap(device, pool->buffer.buffer);

	return pool;
}

void egBufferPoolDestroy(EgBufferPool *pool)
{
    RgDevice *device = egEngineGetDevice(pool->engine);

    rgBufferUnmap(device, pool->buffer.buffer);
	egEngineFreeStorageBuffer(pool->engine, &pool->buffer);

	egFree(pool->allocator, pool);
}

uint32_t egBufferPoolGetBufferIndex(EgBufferPool *pool)
{
	return pool->buffer.index;
}

void egBufferPoolReset(EgBufferPool *pool)
{
	pool->frame_index = (pool->frame_index + 1) % 2;

	if (pool->frame_index == 0)
	{
		pool->allocated_items = 0;
	}
}

uint32_t egBufferPoolAllocateItem(EgBufferPool *pool, size_t size, void *data)
{
	EG_ASSERT(size == pool->item_size);
	size_t item_index = pool->allocated_items++;

	uint8_t *dest = pool->mapping + (item_index * pool->item_size);
	memcpy(dest, data, size);

	return (uint32_t)item_index;
}

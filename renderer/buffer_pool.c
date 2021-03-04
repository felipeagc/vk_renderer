#include "buffer_pool.h"

#include <string.h>
#include "rg.h"
#include "allocator.h"
#include "platform.h"
#include "engine.h"

struct BufferPool
{
    Allocator *allocator;
    Engine *engine;

	uint32_t frame_index;

	size_t item_size;
	size_t item_count;

	BufferHandle buffer;
	size_t allocated_items;

    uint8_t *mapping;
};

BufferPool *BufferPoolCreate(Allocator *allocator, Engine *engine, size_t item_size, size_t item_count)
{
	BufferPool *pool = Allocate(allocator, sizeof(*pool));
	memset(pool, 0, sizeof(*pool));

    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

	pool->allocator = allocator;
	pool->engine = engine;
	pool->item_size = item_size;
	pool->item_count = item_count;

    RgBufferInfo buffer_info = {};
    buffer_info.size = item_size * item_count * 2;
    buffer_info.usage = RG_BUFFER_USAGE_STORAGE | RG_BUFFER_USAGE_TRANSFER_DST;
    buffer_info.memory = RG_BUFFER_MEMORY_HOST;
    pool->buffer = EngineAllocateStorageBufferHandle(engine, &buffer_info);

    pool->mapping = (uint8_t*)rgBufferMap(device, pool->buffer.buffer);

	return pool;
}

void BufferPoolDestroy(BufferPool *pool)
{
    Platform *platform = EngineGetPlatform(pool->engine);
    RgDevice *device = PlatformGetDevice(platform);

    rgBufferUnmap(device, pool->buffer.buffer);
	EngineFreeStorageBufferHandle(pool->engine, &pool->buffer);

	Free(pool->allocator, pool);
}

uint32_t BufferPoolGetBufferIndex(BufferPool *pool)
{
	return pool->buffer.index;
}

void BufferPoolReset(BufferPool *pool)
{
	pool->frame_index = (pool->frame_index + 1) % 2;

	if (pool->frame_index == 0)
	{
		pool->allocated_items = 0;
	}
}

uint32_t BufferPoolAllocateItem(BufferPool *pool, size_t size, void *data)
{
	EG_ASSERT(size == pool->item_size);
	size_t item_index = pool->allocated_items++;

	uint8_t *dest = pool->mapping + (item_index * pool->item_size);
	memcpy(dest, data, size);

	return (uint32_t)item_index;
}

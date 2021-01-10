#include "uniform_arena.h"

#include <assert.h>
#include <string.h>
#include <rg.h>
#include "platform.h"
#include "allocator.h"

struct UniformArena
{
    Platform *platform;
    Allocator *allocator;
    RgBuffer *buffer;
    uint8_t *mapping;
    size_t size;
    size_t offset;
    size_t alignment;
};

static inline size_t alignTo(size_t n, size_t to)
{
    size_t rest = n % to;
    return (rest != 0) ? (n + to - rest) : n;
}

UniformArena *UniformArenaCreate(Platform *platform, Allocator *allocator)
{
    UniformArena *arena =
        (UniformArena*)Allocate(allocator, sizeof(UniformArena));
    *arena = {};

    arena->platform = platform;
    arena->allocator = allocator;

    RgDevice *device = PlatformGetDevice(arena->platform);
    RgLimits limits;
    rgDeviceGetLimits(device, &limits);

    arena->size = 65536;
    arena->alignment = limits.min_uniform_buffer_offset_alignment;

    RgBufferInfo buffer_info = {};
    buffer_info.size = arena->size;
    buffer_info.usage = RG_BUFFER_USAGE_UNIFORM | RG_BUFFER_USAGE_TRANSFER_DST;
    buffer_info.memory = RG_BUFFER_MEMORY_HOST;
    arena->buffer = rgBufferCreate(device, &buffer_info);

    arena->mapping = (uint8_t*)rgBufferMap(device, arena->buffer);

    return arena;
}

void UniformArenaDestroy(UniformArena *arena)
{
    RgDevice *device = PlatformGetDevice(arena->platform);
    rgBufferUnmap(device, arena->buffer);
    rgBufferDestroy(device, arena->buffer);
    Free(arena->allocator, arena);
}

void UniformArenaReset(UniformArena *arena)
{
    arena->offset = 0;
}

void *UniformArenaUse(UniformArena *arena, uint32_t *offset, size_t size)
{
    arena->offset = alignTo(arena->offset, arena->alignment);
    *offset = arena->offset;
    arena->offset += size;
    assert(arena->offset <= arena->size);
    return arena->mapping + *offset;
}

RgBuffer *UniformArenaGetBuffer(UniformArena *arena)
{
    return arena->buffer;
}

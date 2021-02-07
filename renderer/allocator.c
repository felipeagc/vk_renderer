#include "allocator.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *Allocate(Allocator *allocator, size_t size)
{
    if (!allocator) return malloc(size);
    return allocator->allocate(allocator, size);
}

void *Reallocate(Allocator *allocator, void *ptr, size_t size)
{
    if (!allocator) return realloc(ptr, size);
    return allocator->reallocate(allocator, ptr, size);
}

void Free(Allocator *allocator, void *ptr)
{
    if (!allocator)
    {
        free(ptr);
        return;
    }
    allocator->free(allocator, ptr);
}

typedef struct ArenaChunk
{
    struct ArenaChunk *prev;
    uint8_t *data;
    size_t offset;
    size_t size;
} ArenaChunk;

struct Arena
{
    Allocator allocator;
    Allocator *parent_allocator;
    ArenaChunk *last_chunk;
};

#define ARENA_PTR_SIZE(ptr) *(((uint64_t*)ptr)-1)

static ArenaChunk *ArenaNewChunk(Arena *arena, ArenaChunk *prev, size_t size)
{
    ArenaChunk *chunk = (ArenaChunk *)
        Allocate(arena->parent_allocator, sizeof(*chunk));
    memset(chunk, 0, sizeof(*chunk));

    chunk->prev = prev;

    chunk->size = size;
    chunk->data = (uint8_t*)Allocate(arena->parent_allocator, chunk->size);

    return chunk;
}

static void *ArenaAllocate(Allocator *allocator, size_t size)
{
    Arena *arena = (Arena*)allocator;

    ArenaChunk *chunk = arena->last_chunk;

    size_t new_offset = chunk->offset;
    while (new_offset % 16 != 0) new_offset++;
    new_offset += 16; // Header
    size_t data_offset = new_offset;
    new_offset += size;

    if (chunk->size <= new_offset)
    {
        size_t new_chunk_size = chunk->size * 2;
        while (new_chunk_size <= (size + 16)) new_chunk_size *= 2;
        arena->last_chunk = ArenaNewChunk(arena, chunk, new_chunk_size);
        return ArenaAllocate(allocator, size);
    }

    uint8_t *ptr = &chunk->data[data_offset];
    ARENA_PTR_SIZE(ptr) = size;

    chunk->offset = new_offset;

    return (void*)ptr;
}

static void *ArenaReallocate(Allocator *allocator, void *ptr, size_t size)
{
    uint64_t old_size = ARENA_PTR_SIZE(ptr);

    void *new_ptr = ArenaAllocate(allocator, size);
    memcpy(new_ptr, ptr, old_size);

    return new_ptr;
}

static void ArenaFree(Allocator *allocator, void *ptr)
{
    (void)allocator;
    (void)ptr;
}

Arena *ArenaCreate(Allocator *parent_allocator, size_t default_size)
{
    Arena *arena = (Arena*)Allocate(parent_allocator, sizeof(*arena));
    memset(arena, 0, sizeof(*arena));

    arena->allocator.allocate = ArenaAllocate;
    arena->allocator.reallocate = ArenaReallocate;
    arena->allocator.free = ArenaFree;

    arena->parent_allocator = parent_allocator;
    arena->last_chunk = ArenaNewChunk(arena, NULL, default_size);

    return arena;
}

Allocator *ArenaGetAllocator(Arena *arena)
{
    return &arena->allocator;
}

void ArenaDestroy(Arena *arena)
{
    ArenaChunk *chunk = arena->last_chunk;
    while (chunk)
    {
        Free(arena->parent_allocator, chunk->data);
        ArenaChunk *chunk_to_free = chunk;
        chunk = chunk->prev;
        Free(arena->parent_allocator, chunk_to_free);
    }
    Free(arena->parent_allocator, arena);
}

const char *Strdup(Allocator *allocator, const char *str)
{
    size_t length = strlen(str);
    char *new_str = (char*)Allocate(allocator, length+1);
    memcpy(new_str, str, length);
    new_str[length] = '\0';
    return new_str;
}

const char *NullTerminate(Allocator *allocator, const char *str, size_t length)
{
    char *new_str = (char*)Allocate(allocator, length+1);
    memcpy(new_str, str, length);
    new_str[length] = '\0';
    return new_str;
}

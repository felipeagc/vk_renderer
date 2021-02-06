#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;

struct Allocator
{
    void *(*allocate)(Allocator *allocator, size_t size);
    void *(*reallocate)(Allocator *allocator, void *ptr, size_t size);
    void (*free)(Allocator *allocator, void *ptr);
};

void *Allocate(Allocator *allocator, size_t size);
void *Reallocate(Allocator *allocator, void *ptr, size_t size);
void Free(Allocator *allocator, void *ptr);

typedef struct Arena Arena;

Arena *ArenaCreate(Allocator *parent_allocator, size_t default_size);
Allocator *ArenaGetAllocator(Arena *arena);
void ArenaReset(Arena *arena);
void ArenaDestroy(Arena *arena);

#ifdef __cplusplus
}
#endif

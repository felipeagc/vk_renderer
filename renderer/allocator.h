#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;

struct EgAllocator
{
    void *(*allocate)(EgAllocator *allocator, size_t size);
    void *(*reallocate)(EgAllocator *allocator, void *ptr, size_t size);
    void (*free)(EgAllocator *allocator, void *ptr);
};

void *egAllocate(EgAllocator *allocator, size_t size);
void *egReallocate(EgAllocator *allocator, void *ptr, size_t size);
void egFree(EgAllocator *allocator, void *ptr);

typedef struct EgArena EgArena;

EgArena *egArenaCreate(EgAllocator *parent_allocator, size_t default_size);
EgAllocator *egArenaGetAllocator(EgArena *arena);
void egArenaDestroy(EgArena *arena);

const char *egStrdup(EgAllocator *allocator, const char *str);
const char *egNullTerminate(EgAllocator *allocator, const char *str, size_t length);

#ifdef __cplusplus
}
#endif

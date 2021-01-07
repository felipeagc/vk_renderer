#include "allocator.h"

#include <stdlib.h>

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
    if (!allocator) return free(ptr);
    return allocator->free(allocator, ptr);
}

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;

struct Allocator
{
    void *user_ptr;
    void *(*allocate)(Allocator *allocator, size_t size);
    void *(*reallocate)(Allocator *allocator, void *ptr, size_t size);
    void (*free)(Allocator *allocator, void *ptr);
};

void *Allocate(Allocator *allocator, size_t size);
void *Reallocate(Allocator *allocator, void *ptr, size_t size);
void Free(Allocator *allocator, void *ptr);

#ifdef __cplusplus
}
#endif

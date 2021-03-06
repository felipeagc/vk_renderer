#pragma once

#include <stddef.h>
#include <stdint.h>
#include "base.h"
#include "allocator.h"

#define EG_ARRAY_HEADER_SIZE (sizeof(uint64_t) * 4)
#define EG_ARRAY_INITIAL_CAPACITY 8

#define EgArray(type) type *

#define egArrayItemSize(arr) ((arr) != NULL ? _egArrayItemSize(arr) : 0)
#define egArrayLength(arr) ((arr) != NULL ? _egArrayLength(arr) : 0)
#define egArrayCapacity(arr) ((arr) != NULL ? _egArrayCapacity(arr) : 0)
#define egArrayAllocator(arr) ((arr) != NULL ? _egArrayAllocator(arr) : 0)

#define _egArrayItemSize(arr) (*((uint64_t *)(arr)-1))
#define _egArrayLength(arr) (*((uint64_t *)(arr)-2))
#define _egArrayCapacity(arr) (*((uint64_t *)(arr)-3))
#define _egArrayAllocator(arr) (*((EgAllocator **)(arr)-4))

#define egArrayCreate(allocator, type) ((type *)_egArrayCreate(allocator, sizeof(type)))
#define egArrayPush(arr, value)                                                          \
    (_egArrayEnsure((void **)arr, egArrayLength(*arr) + 1),                              \
     (*arr)[_egArrayLength(*arr)++] = value)
#define egArrayPop(arr) ((egArrayLength(*arr) > 0) ? (--_egArrayLength(*arr)) : 0)
#define egArrayEnsure(arr, wanted_capacity) _egArrayEnsure((void **)arr, wanted_capacity)
#define egArrayResize(arr, wanted_size)                                                  \
    (_egArrayEnsure((void **)arr, wanted_size), _egArrayLength(*arr) = wanted_size)
#define egArrayFree(arr) ((*arr) != NULL ? (_egArrayFree(*(arr)), (*(arr)) = NULL) : 0)

#define egArrayFor(arr, index) for (size_t index = 0; index < egArrayLength(arr); ++index)

EG_INLINE static void *_egArrayCreate(EgAllocator *allocator, size_t item_size)
{
    void *ptr =
        ((uint64_t *)egAllocate(
            allocator, EG_ARRAY_HEADER_SIZE + (item_size * EG_ARRAY_INITIAL_CAPACITY))) +
        4;

    _egArrayItemSize(ptr) = item_size;
    _egArrayAllocator(ptr) = allocator;
    _egArrayLength(ptr) = 0;
    _egArrayCapacity(ptr) = EG_ARRAY_INITIAL_CAPACITY;

    return ptr;
}

EG_INLINE static void _egArrayFree(void *arr)
{
    EgAllocator *allocator = egArrayAllocator(arr);
    egFree(allocator, ((uint64_t *)arr) - 4);
}

EG_INLINE static void _egArrayEnsure(void **arr_ptr, size_t wanted_capacity)
{
    void *arr = *arr_ptr;

    size_t item_size = egArrayItemSize(arr);
    EgAllocator *allocator = egArrayAllocator(arr);
    size_t array_capacity = egArrayCapacity(arr);

    if (wanted_capacity > array_capacity)
    {
        array_capacity *= 2;
        if (array_capacity < wanted_capacity) array_capacity = wanted_capacity;

        arr = ((uint64_t *)egReallocate(
                  allocator,
                  ((uint64_t *)arr - 4),
                  EG_ARRAY_HEADER_SIZE + (item_size * array_capacity))) +
              4;
        _egArrayCapacity(arr) = array_capacity;
    }

    *arr_ptr = arr;
}

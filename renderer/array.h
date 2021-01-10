#pragma once

#include <stddef.h>
#include "allocator.h"

template <typename T>
struct Array
{
    T *ptr = nullptr;
    size_t length = 0;
    size_t capacity = 0;
    Allocator *allocator = nullptr;

    T &operator[](size_t index)
    {
        return this->ptr[index];
    }

    static inline Array with_allocator(Allocator *allocator)
    {
        Array array = {};
        array.allocator = allocator;
        return array;
    }

    void ensure(size_t wanted_capacity)
    {
        if (this->capacity == 0)
        {
            this->capacity = wanted_capacity;
            if (this->ptr) Free(this->allocator, this->ptr);
            this->ptr = (T*)Allocate(this->allocator, sizeof(T) * wanted_capacity);
            return;
        }

        if (wanted_capacity > this->capacity)
        {
            this->capacity = wanted_capacity;
            this->ptr = (T*)Reallocate(this->allocator, this->ptr, sizeof(T) * wanted_capacity);
        }
    }

    void push_back(const T &value)
    {
        this->ensure(this->length + 1);
        this->ptr[this->length] = value;
        this->length++;
    }

    void free()
    {
        Free(this->allocator, this->ptr);
        this->ptr = nullptr;
        this->length = 0;
        this->capacity = 0;
    }

    T *begin()
    {
        return this->ptr;
    }

    T *end()
    {
        return this->ptr + this->length;
    }
};

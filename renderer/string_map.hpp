#pragma once

#include <stdint.h>
#include <string.h>
#include "allocator.h"

static inline uint64_t StringMapHash(const char *string)
{
    uint64_t hash = 14695981039346656037ULL;
    while (*string)
    {
        hash = ((hash) * 1099511628211) ^ (*string);
        ++string;
    }
    return hash;
}

template <typename T>
struct StringMap
{
    struct Slot
    {
        const char *key;
        uint64_t hash;
        T value;
    };

    struct Iterator
    {
        StringMap *map;
        uint64_t index;

    public:
        Slot &operator*() const { return map->slots[this->index]; }

        // Prefix increment
        Iterator& operator++()
        {
            this->index++;
            for (; this->index < this->map->size; ++this->index)
            {
                if (this->map->slots[this->index].hash != 0) break;
            }
            return *this;
        }  

        // Postfix increment
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

        friend bool operator== (const Iterator& a, const Iterator& b) { return a.index == b.index; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a.index != b.index; };
    };

    Allocator *allocator = nullptr;
    Slot *slots = nullptr;
    uint64_t size = 0;

    void grow();

    static inline StringMap create(Allocator *allocator, uint64_t size = 16)
    {
        StringMap map = {};
        map.allocator = allocator;
        map.size = size;

        map.size -= 1;
        map.size |= map.size >> 1;
        map.size |= map.size >> 2;
        map.size |= map.size >> 4;
        map.size |= map.size >> 8;
        map.size |= map.size >> 16;
        map.size |= map.size >> 32;
        map.size += 1;

        map.slots = (Slot*)Allocate(map.allocator, sizeof(*map.slots) * map.size);
        memset(map.slots, 0, sizeof(*map.slots) * map.size);

        return map;
    }

    void set(const char *key, T value)
    {
        uint64_t hash = StringMapHash(key);
        uint64_t i = hash & (this->size - 1);
        uint64_t iters = 0;

        while ((this->slots[i].hash != hash || strcmp(this->slots[i].key, key) != 0) &&
               this->slots[i].hash != 0 && iters < this->size)
        {
            i = (i + 1) & (this->size - 1);
            iters++;
        }

        if (iters >= this->size)
        {
            this->grow();
            return this->set(key, value);
        }

        this->slots[i].key = key;
        this->slots[i].value = value;
        this->slots[i].hash = hash;
    }

    bool get(const char *key, T *value)
    {
        uint64_t hash = StringMapHash(key);
        uint64_t i = hash & (this->size - 1);
        uint64_t iters = 0;

        while ((this->slots[i].hash != hash || strcmp(this->slots[i].key, key) != 0) &&
               this->slots[i].hash != 0 && iters < this->size)
        {
            i = (i + 1) & (this->size - 1);
            iters++;
        }

        if (iters >= this->size)
        {
            return false;
        }

        if (this->slots[i].hash != 0)
        {
            if (value) *value = this->slots[i].value;
            return true;
        }

        return false;
    }

    void remove(const char *key)
    {
        uint64_t hash = StringMapHash(key);
        uint64_t i = hash & (this->size - 1);
        uint64_t iters = 0;

        while ((this->slots[i].hash != hash || strcmp(this->slots[i].key, key) != 0) &&
               this->slots[i].hash != 0 && iters < this->size)
        {
            i = (i + 1) & (this->size - 1);
            iters++;
        }

        if (iters >= this->size)
        {
            return;
        }

        this->slots[i].hash = 0;
        this->slots[i].key = nullptr;
    }

    void free()
    {
        Free(this->allocator, this->slots);
    }

    Iterator begin()
    {
        for (uint64_t i = 0; i < this->size; ++i)
        {
            if (this->slots[i].hash != 0)
            {
                return Iterator{this, i};
            }
        }
        return Iterator{this, this->size};
    }

    Iterator end()
    {
        return Iterator{this, this->size};
    }

    size_t length()
    {
        size_t count = 0;
        for (auto &slot : *this)
        {
            (void)slot;
            count++;
        }
        return count;
    }

};

template <typename T>
void StringMap<T>::grow()
{
    uint64_t old_size = this->size;
    Slot *old_slots = this->slots;

    this->size = old_size * 2;
    this->slots = (Slot*)Allocate(this->allocator, sizeof(*this->slots) * this->size);
    memset(this->slots, 0, sizeof(*this->slots) * this->size);

    for (uint64_t i = 0; i < old_size; i++)
    {
        if (old_slots[i].hash != 0)
        {
            this->set(old_slots[i].key, old_slots[i].value);
        }
    }

    Free(this->allocator, this->slots);
}

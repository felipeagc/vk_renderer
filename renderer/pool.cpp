#include "pool.h"

#include "allocator.h"
#include "array.hpp"

struct Pool
{
    Allocator *allocator;
    uint32_t slot_count;
    uint32_t new_slot_index;
    Array<uint32_t> free_slots;
};

extern "C" Pool *PoolCreate(Allocator *allocator, uint32_t slot_count)
{
    Pool *pool = (Pool*)Allocate(allocator, sizeof(*pool));
    *pool = {};

    pool->allocator = allocator;
    pool->slot_count = slot_count;
    pool->new_slot_index = 0;
    pool->free_slots = Array<uint32_t>::create(allocator);

    return pool;
}

extern "C" void PoolDestroy(Pool *pool)
{
    pool->free_slots.free();
    Free(pool->allocator, pool);
}

extern "C" uint32_t PoolGetSlotCount(Pool *pool)
{
    return pool->slot_count;
}

extern "C" uint32_t PoolGetFreeSlotCount(Pool *pool)
{
    return (pool->slot_count - pool->new_slot_index) + (uint32_t)pool->free_slots.length;
}

extern "C" uint32_t PoolAllocateSlot(Pool *pool)
{
    if (pool->free_slots.length > 0)
    {
        uint32_t slot = pool->free_slots[pool->free_slots.length-1];
        pool->free_slots.pop_back();
        return slot;
    }

    if (pool->new_slot_index < pool->slot_count)
    {
        return pool->new_slot_index++;
    }

    return UINT32_MAX;
}

extern "C" void PoolFreeSlot(Pool *pool, uint32_t slot)
{
    pool->free_slots.push_back(slot);
}

#include "pool.h"

#include "allocator.h"
#include "array.h"

struct EgPool
{
    EgAllocator *allocator;
    uint32_t slot_count;
    uint32_t new_slot_index;
    EgArray(uint32_t) free_slots;
};

EgPool *egPoolCreate(EgAllocator *allocator, uint32_t slot_count)
{
    EgPool *pool = (EgPool *)egAllocate(allocator, sizeof(*pool));
    *pool = (EgPool){
        .allocator = allocator,
        .slot_count = slot_count,
        .new_slot_index = 0,
        .free_slots = egArrayCreate(allocator, uint32_t),
    };

    return pool;
}

void egPoolDestroy(EgPool *pool)
{
    egArrayFree(&pool->free_slots);
    egFree(pool->allocator, pool);
}

uint32_t egPoolGetSlotCount(EgPool *pool)
{
    return pool->slot_count;
}

uint32_t egPoolGetFreeSlotCount(EgPool *pool)
{
    return (pool->slot_count - pool->new_slot_index) +
           (uint32_t)egArrayLength(pool->free_slots);
}

uint32_t egPoolAllocateSlot(EgPool *pool)
{
    if (egArrayLength(pool->free_slots) > 0)
    {
        uint32_t slot = pool->free_slots[egArrayLength(pool->free_slots) - 1];
        egArrayPop(&pool->free_slots);
        return slot;
    }

    if (pool->new_slot_index < pool->slot_count)
    {
        return pool->new_slot_index++;
    }

    return UINT32_MAX;
}

void egPoolFreeSlot(EgPool *pool, uint32_t slot)
{
    egArrayPush(&pool->free_slots, slot);
}

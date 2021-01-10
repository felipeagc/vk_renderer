#pragma once

#include <stdint.h>
#include <stddef.h>

typedef union RgDescriptor RgDescriptor;
typedef struct Platform Platform;
typedef struct Allocator Allocator;
typedef struct UniformArena UniformArena;
typedef struct RgBuffer RgBuffer;

UniformArena *UniformArenaCreate(Platform *platform, Allocator *allocator);
void UniformArenaDestroy(UniformArena *arena);
void UniformArenaReset(UniformArena *arena);
void *UniformArenaUse(UniformArena *arena, uint32_t *offset, size_t size);
RgBuffer *UniformArenaGetBuffer(UniformArena *arena);

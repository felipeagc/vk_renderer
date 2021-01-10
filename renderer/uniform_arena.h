#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct Engine Engine;
typedef struct UniformArena UniformArena;
typedef struct RgBuffer RgBuffer;

UniformArena *UniformArenaCreate(Allocator *allocator, Engine *engine);
void UniformArenaDestroy(UniformArena *arena);
void UniformArenaReset(UniformArena *arena);
void *UniformArenaUse(UniformArena *arena, uint32_t *offset, size_t size);
RgBuffer *UniformArenaGetBuffer(UniformArena *arena);

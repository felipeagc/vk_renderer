#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct Engine Engine;
typedef union Mat4 Mat4;
typedef struct Mesh Mesh;
typedef struct ModelAsset ModelAsset;
typedef struct RgCmdBuffer RgCmdBuffer;
typedef struct UniformArena UniformArena;

ModelAsset *ModelAssetFromGltf(
        Allocator *allocator,
        Engine *engine,
        UniformArena *uniform_arena,
        const uint8_t *data,
        size_t size);
ModelAsset *ModelAssetFromMesh(
        Allocator *allocator,
        Engine *engine,
        UniformArena *uniform_arena,
        Mesh *mesh);
void ModelAssetDestroy(ModelAsset *model);
void ModelAssetRender(ModelAsset *model, RgCmdBuffer *cmd_buffer, Mat4 *transform);

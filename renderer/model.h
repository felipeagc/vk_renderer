#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct Platform Platform;
typedef union Mat4 Mat4;
typedef struct Mesh Mesh;
typedef struct Model Model;
typedef struct RgCmdBuffer RgCmdBuffer;
typedef struct UniformArena UniformArena;

Model *ModelFromGltf(
        Allocator *allocator,
        Platform *platform,
        UniformArena *uniform_arena,
        const uint8_t *data,
        size_t size);
Model *ModelFromMesh(
        Allocator *allocator,
        Platform *platform,
        UniformArena *uniform_arena,
        Mesh *mesh);
void ModelDestroy(Model *model);
void ModelRender(Model *model, RgCmdBuffer *cmd_buffer, Mat4 *transform);

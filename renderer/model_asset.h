#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Engine Engine;
typedef union Mat4 Mat4;
typedef struct Mesh Mesh;
typedef struct RgCmdBuffer RgCmdBuffer;
typedef struct BufferPool BufferPool;
typedef struct CameraUniform CameraUniform;

typedef struct ModelManager ModelManager;
typedef struct ModelAsset ModelAsset;

ModelManager *ModelManagerCreate(
		Allocator *allocator, Engine *engine, size_t model_limit, size_t material_limit);
void ModelManagerDestroy(ModelManager *manager);

void ModelManagerBeginFrame(ModelManager *manager, CameraUniform *camera_uniform);

ModelAsset *ModelAssetFromGltf(
        ModelManager *manager,
        const uint8_t *data,
        size_t size);
ModelAsset *ModelAssetFromMesh(
        ModelManager *manager,
        Mesh *mesh);
void ModelAssetDestroy(ModelAsset *model);
void ModelAssetRender(ModelAsset *model, RgCmdBuffer *cmd_buffer, Mat4 *transform);

#ifdef __cplusplus
}
#endif

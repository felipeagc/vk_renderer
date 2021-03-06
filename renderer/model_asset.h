#pragma once

#include <stdint.h>
#include <stddef.h>
#include "math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgAllocator EgAllocator;
typedef struct EgEngine EgEngine;
typedef struct EgMesh EgMesh;
typedef struct RgCmdBuffer RgCmdBuffer;
typedef struct EgBufferPool EgBufferPool;
typedef struct EgCameraUniform EgCameraUniform;

typedef struct EgModelManager EgModelManager;
typedef struct EgModelAsset EgModelAsset;

EgModelManager *egModelManagerCreate(
		EgAllocator *allocator, EgEngine *engine, size_t model_limit, size_t material_limit);
void egModelManagerDestroy(EgModelManager *manager);

void egModelManagerBeginFrame(EgModelManager *manager, EgCameraUniform *camera_uniform);

EgModelAsset *egModelAssetFromGltf(
        EgModelManager *manager,
        const uint8_t *data,
        size_t size);
EgModelAsset *egModelAssetFromMesh(
        EgModelManager *manager,
        EgMesh *mesh);
void egModelAssetDestroy(EgModelAsset *model);
void egModelAssetRender(EgModelAsset *model, RgCmdBuffer *cmd_buffer, float4x4 *transform);

#ifdef __cplusplus
}
#endif

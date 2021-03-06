#pragma once

#include <rg.h>
#include "math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgEngine EgEngine;
typedef struct EgMesh EgMesh;
typedef struct EgAllocator EgAllocator;

EgMesh *egMeshCreateCube(EgAllocator *allocator, EgEngine *engine, RgCmdPool *cmd_pool);
EgMesh *egMeshCreateUVSphere(
        EgAllocator *allocator,
        EgEngine *engine,
        RgCmdPool *cmd_pool,
        float radius,
        uint32_t divisions);
void egMeshDestroy(EgMesh *mesh);

RgBuffer *egMeshGetVertexBuffer(EgMesh* mesh);
RgBuffer *egMeshGetIndexBuffer(EgMesh* mesh);
uint32_t egMeshGetIndexCount(EgMesh* mesh);

#ifdef __cplusplus
}
#endif

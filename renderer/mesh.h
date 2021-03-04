#pragma once

#include <rg.h>
#include "math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Engine Engine;
typedef struct Mesh Mesh;
typedef struct Allocator Allocator;

Mesh *MeshCreateCube(Allocator *allocator, Engine *engine, RgCmdPool *cmd_pool);
Mesh *MeshCreateUVSphere(
        Allocator *allocator,
        Engine *engine,
        RgCmdPool *cmd_pool,
        float radius,
        uint32_t divisions);
void MeshDestroy(Mesh *mesh);

RgBuffer *MeshGetVertexBuffer(Mesh* mesh);
RgBuffer *MeshGetIndexBuffer(Mesh* mesh);
uint32_t MeshGetIndexCount(Mesh* mesh);

#ifdef __cplusplus
}
#endif

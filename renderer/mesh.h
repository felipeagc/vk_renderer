#pragma once

#include <rg.h>
#include "math.h"

typedef struct Engine Engine;
typedef struct Mesh Mesh;
typedef struct Allocator Allocator;

typedef struct Vertex
{
    Vec3 pos;
    Vec3 normal;
    Vec4 tangent;
    Vec2 uv;
} Vertex;

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

#pragma once

#include <rg.h>
#include "math.h"

typedef struct Platform Platform;
typedef struct Mesh Mesh;
typedef struct Allocator Allocator;

typedef struct Vertex
{
    Vec3 pos;
    Vec3 normal;
    Vec4 tangent;
    Vec2 uv;
} Vertex;

Mesh *MeshCreateCube(Platform *platform, RgCmdPool *cmd_pool, Allocator *allocator);
Mesh *MeshCreateUVSphere(
        Platform *platform,
        RgCmdPool *cmd_pool,
        Allocator *allocator,
        float radius,
        uint32_t divisions);
void MeshDestroy(Mesh *mesh);

RgBuffer *MeshGetVertexBuffer(Mesh* mesh);
RgBuffer *MeshGetIndexBuffer(Mesh* mesh);
uint32_t MeshGetIndexCount(Mesh* mesh);

#include "mesh.h"

#include "allocator.h"
#include "platform.h"

struct Mesh
{
    Allocator *allocator;
    Platform *platform;
    RgBuffer *vertex_buffer;
    RgBuffer *index_buffer;
    uint32_t index_count;
};

Mesh *MeshCreateCube(Platform *platform, RgCmdPool *cmd_pool, Allocator *allocator)
{
    Mesh *mesh = (Mesh*)Allocate(allocator, sizeof(Mesh));
    *mesh = {};
    mesh->allocator = allocator;
    mesh->platform = platform;

    Vertex vertices[8] = {
        {V3( 0.5, 0.5,  0.5), {}, {}, {}},
        {V3(-0.5, 0.5,  0.5), {}, {}, {}},
        {V3(-0.5, 0.5, -0.5), {}, {}, {}},
        {V3( 0.5, 0.5, -0.5), {}, {}, {}},

        {V3( 0.5, -0.5,  0.5), {}, {}, {}},
        {V3(-0.5, -0.5,  0.5), {}, {}, {}},
        {V3(-0.5, -0.5, -0.5), {}, {}, {}},
        {V3( 0.5, -0.5, -0.5), {}, {}, {}},
    };

    uint32_t indices[36] = {
        0, 3, 2,
        2, 1, 0,

        6, 7, 4,
        4, 5, 6,

        6, 2, 3,
        3, 7, 6,

        7, 3, 0,
        0, 4, 7,

        4, 0, 1,
        1, 5, 4,

        5, 1, 2,
        2, 6, 5,
    };

    RgBufferInfo vertex_buffer_info = {};
    vertex_buffer_info.size = sizeof(vertices);
    vertex_buffer_info.usage = RG_BUFFER_USAGE_VERTEX | RG_BUFFER_USAGE_TRANSFER_DST;
    vertex_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;

    RgBufferInfo index_buffer_info = {};
    index_buffer_info.size = sizeof(indices);
    index_buffer_info.usage = RG_BUFFER_USAGE_INDEX | RG_BUFFER_USAGE_TRANSFER_DST;
    index_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;

    RgDevice *device = PlatformGetDevice(platform);

    mesh->vertex_buffer = rgBufferCreate(device, &vertex_buffer_info);
    mesh->index_buffer = rgBufferCreate(device, &index_buffer_info);

    rgBufferUpload(device, cmd_pool, mesh->vertex_buffer, 0, sizeof(vertices), vertices);
    rgBufferUpload(device, cmd_pool, mesh->index_buffer, 0, sizeof(indices), indices);

    mesh->index_count = sizeof(indices) / sizeof(indices[0]);

    return mesh;
}

void MeshDestroy(Mesh *mesh)
{
    RgDevice *device = PlatformGetDevice(mesh->platform);

    rgBufferDestroy(device, mesh->vertex_buffer);
    rgBufferDestroy(device, mesh->index_buffer);

    Free(mesh->allocator, mesh);
}

RgBuffer *MeshGetVertexBuffer(Mesh* mesh)
{
    return mesh->vertex_buffer;
}

RgBuffer *MeshGetIndexBuffer(Mesh* mesh)
{
    return mesh->index_buffer;
}

uint32_t MeshGetIndexCount(Mesh* mesh)
{
    return mesh->index_count;
}

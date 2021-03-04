#include "mesh.h"

#include "math.h"
#include "allocator.h"
#include "platform.h"
#include "engine.h"
#include "array.hpp"

struct Mesh
{
    Allocator *allocator;
    Engine *engine;
    RgBuffer *vertex_buffer;
    RgBuffer *index_buffer;
    uint32_t index_count;
};

Mesh *MeshCreateCube(Allocator *allocator, Engine *engine, RgCmdPool *cmd_pool)
{
    Mesh *mesh = (Mesh*)Allocate(allocator, sizeof(Mesh));
    *mesh = {};
    mesh->allocator = allocator;
    mesh->engine = engine;

    Platform *platform = EngineGetPlatform(engine);

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

Mesh *MeshCreateUVSphere(
        Allocator *allocator,
        Engine *engine,
        RgCmdPool *cmd_pool,
        float radius,
        uint32_t divisions)
{
    Mesh *mesh = (Mesh*)Allocate(allocator, sizeof(Mesh));
    *mesh = {};
    mesh->allocator = allocator;
    mesh->engine = engine;

    Platform *platform = EngineGetPlatform(engine);

    Array<Vertex> vertices = Array<Vertex>::create(allocator);
    Array<uint32_t> indices = Array<uint32_t>::create(allocator);

    float step = 1.0f / (float)divisions;
    Vec3 step3 = V3(step, step, step);

    Vec3 origins[6] = {
        V3(-1.0, -1.0, -1.0),
        V3(1.0, -1.0, -1.0),
        V3(1.0, -1.0, 1.0),
        V3(-1.0, -1.0, 1.0),
        V3(-1.0, 1.0, -1.0),
        V3(-1.0, -1.0, 1.0),
    };
    Vec3 rights[6] = {
        V3(2.0, 0.0, 0.0),
        V3(0.0, 0.0, 2.0),
        V3(-2.0, 0.0, 0.0),
        V3(0.0, 0.0, -2.0),
        V3(2.0, 0.0, 0.0),
        V3(2.0, 0.0, 0.0),
    };
    Vec3 ups[6] = {
        V3(0.0, 2.0, 0.0),
        V3(0.0, 2.0, 0.0),
        V3(0.0, 2.0, 0.0),
        V3(0.0, 2.0, 0.0),
        V3(0.0, 0.0, 2.0),
        V3(0.0, 0.0, -2.0),
    };

    {
        for (uint32_t face = 0; face < 6; ++face)
        {
            Vec3 origin = origins[face];
            Vec3 right = rights[face];
            Vec3 up = ups[face];

            for (uint32_t j = 0; j < divisions + 1; ++j)
            {
                Vec3 jv = V3((float)j, (float)j, (float)j);

                for (uint32_t i = 0; i < divisions + 1; ++i)
                {
                    Vec3 iv = V3((float)i, (float)i, (float)i);

                    Vec3 p = origin + (step3 * (iv * right + jv * up));
                    p = Vec3Normalize(p) * radius;

                    vertices.push_back(Vertex{p, {}, {}, {}});
                }
            }
        }
    }

    uint32_t k = divisions + 1;

    {
        for (uint32_t face = 0; face < 6; ++face)
        {
            for (uint32_t j = 0; j < divisions; ++j)
            {
                bool bottom = j < (divisions / 2);

                for (uint32_t i = 0; i < divisions; ++i)
                {
                    bool left = i < (divisions / 2);
                    uint32_t a = (face * k + j) * k + i;
                    uint32_t b = (face * k + j) * k + i + 1;
                    uint32_t c = (face * k + j + 1) * k + i;
                    uint32_t d = (face * k + j + 1) * k + i + 1;
                    if ((bottom ^ left) != 0)
                    {
                        indices.push_back(a);
                        indices.push_back(c);
                        indices.push_back(b);
                        indices.push_back(c);
                        indices.push_back(d);
                        indices.push_back(b);
                    }
                    else
                    {
                        indices.push_back(a);
                        indices.push_back(c);
                        indices.push_back(d);
                        indices.push_back(a);
                        indices.push_back(d);
                        indices.push_back(b);
                    }
                }
            }
        }
    }

    size_t vertices_size = vertices.length * sizeof(Vertex);
    size_t indices_size = indices.length * sizeof(uint32_t);

    RgBufferInfo vertex_buffer_info = {};
    vertex_buffer_info.size = vertices_size;
    vertex_buffer_info.usage = RG_BUFFER_USAGE_VERTEX | RG_BUFFER_USAGE_TRANSFER_DST;
    vertex_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;

    RgBufferInfo index_buffer_info = {};
    index_buffer_info.size = indices_size;
    index_buffer_info.usage = RG_BUFFER_USAGE_INDEX | RG_BUFFER_USAGE_TRANSFER_DST;
    index_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;

    RgDevice *device = PlatformGetDevice(platform);

    mesh->vertex_buffer = rgBufferCreate(device, &vertex_buffer_info);
    mesh->index_buffer = rgBufferCreate(device, &index_buffer_info);

    rgBufferUpload(device, cmd_pool, mesh->vertex_buffer, 0, vertices_size, vertices.ptr);
    rgBufferUpload(device, cmd_pool, mesh->index_buffer, 0, indices_size, indices.ptr);

    mesh->index_count = (uint32_t)indices.length;

    indices.free();
    vertices.free();
    return mesh;
}

void MeshDestroy(Mesh *mesh)
{
    Platform *platform = EngineGetPlatform(mesh->engine);
    RgDevice *device = PlatformGetDevice(platform);

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

#include "mesh.h"

#include "math.h"
#include "allocator.h"
#include "engine.h"
#include "array.h"

struct EgMesh
{
    EgAllocator *allocator;
    EgEngine *engine;
    RgBuffer *vertex_buffer;
    RgBuffer *index_buffer;
    uint32_t index_count;
};

EgMesh *egMeshCreateCube(EgAllocator *allocator, EgEngine *engine, RgCmdPool *cmd_pool)
{
    EgMesh *mesh = (EgMesh*)egAllocate(allocator, sizeof(EgMesh));
    *mesh = (EgMesh){0};

    mesh->allocator = allocator;
    mesh->engine = engine;

    EgVertex vertices[8] = {
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

    RgDevice *device = egEngineGetDevice(engine);

    mesh->vertex_buffer = rgBufferCreate(device, &vertex_buffer_info);
    mesh->index_buffer = rgBufferCreate(device, &index_buffer_info);

    rgBufferUpload(device, cmd_pool, mesh->vertex_buffer, 0, sizeof(vertices), vertices);
    rgBufferUpload(device, cmd_pool, mesh->index_buffer, 0, sizeof(indices), indices);

    mesh->index_count = sizeof(indices) / sizeof(indices[0]);

    return mesh;
}

EgMesh *egMeshCreateUVSphere(
        EgAllocator *allocator,
        EgEngine *engine,
        RgCmdPool *cmd_pool,
        float radius,
        uint32_t divisions)
{
    EgMesh *mesh = (EgMesh*)egAllocate(allocator, sizeof(EgMesh));
    *mesh = (EgMesh){0};
    mesh->allocator = allocator;
    mesh->engine = engine;

    EgArray(EgVertex) vertices = egArrayCreate(allocator, EgVertex);
    EgArray(uint32_t) indices = egArrayCreate(allocator, uint32_t);

    float step = 1.0f / (float)divisions;
    float3 step3 = V3(step, step, step);

    float3 origins[6] = {
        V3(-1.0, -1.0, -1.0),
        V3(1.0, -1.0, -1.0),
        V3(1.0, -1.0, 1.0),
        V3(-1.0, -1.0, 1.0),
        V3(-1.0, 1.0, -1.0),
        V3(-1.0, -1.0, 1.0),
    };
    float3 rights[6] = {
        V3(2.0, 0.0, 0.0),
        V3(0.0, 0.0, 2.0),
        V3(-2.0, 0.0, 0.0),
        V3(0.0, 0.0, -2.0),
        V3(2.0, 0.0, 0.0),
        V3(2.0, 0.0, 0.0),
    };
    float3 ups[6] = {
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
            float3 origin = origins[face];
            float3 right = rights[face];
            float3 up = ups[face];

            for (uint32_t j = 0; j < divisions + 1; ++j)
            {
                float3 jv = V3((float)j, (float)j, (float)j);

                for (uint32_t i = 0; i < divisions + 1; ++i)
                {
                    float3 iv = V3((float)i, (float)i, (float)i);

                    float3 ivright = egFloat3Mul(iv, right);
                    float3 jvup = egFloat3Mul(jv, up);
                    float3 sum = egFloat3Add(ivright, jvup);

                    float3 p = egFloat3Add(origin, egFloat3Mul(step3, sum));
                    p = egFloat3MulScalar(egFloat3Normalize(p), radius);


                    EgVertex vertex = {p, {}, {}, {}};
                    egArrayPush(&vertices, vertex);
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
                        egArrayPush(&indices, a);
                        egArrayPush(&indices, c);
                        egArrayPush(&indices, b);
                        egArrayPush(&indices, c);
                        egArrayPush(&indices, d);
                        egArrayPush(&indices, b);
                    }
                    else
                    {
                        egArrayPush(&indices, a);
                        egArrayPush(&indices, c);
                        egArrayPush(&indices, d);
                        egArrayPush(&indices, a);
                        egArrayPush(&indices, d);
                        egArrayPush(&indices, b);
                    }
                }
            }
        }
    }

    size_t vertices_size = egArrayLength(vertices) * sizeof(EgVertex);
    size_t indices_size = egArrayLength(indices) * sizeof(uint32_t);

    RgBufferInfo vertex_buffer_info = {};
    vertex_buffer_info.size = vertices_size;
    vertex_buffer_info.usage = RG_BUFFER_USAGE_VERTEX | RG_BUFFER_USAGE_TRANSFER_DST;
    vertex_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;

    RgBufferInfo index_buffer_info = {};
    index_buffer_info.size = indices_size;
    index_buffer_info.usage = RG_BUFFER_USAGE_INDEX | RG_BUFFER_USAGE_TRANSFER_DST;
    index_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;

    RgDevice *device = egEngineGetDevice(mesh->engine);

    mesh->vertex_buffer = rgBufferCreate(device, &vertex_buffer_info);
    mesh->index_buffer = rgBufferCreate(device, &index_buffer_info);

    rgBufferUpload(device, cmd_pool, mesh->vertex_buffer, 0, vertices_size, vertices);
    rgBufferUpload(device, cmd_pool, mesh->index_buffer, 0, indices_size, indices);

    mesh->index_count = (uint32_t)egArrayLength(indices);

    egArrayFree(&indices);
    egArrayFree(&vertices);
    return mesh;
}

void egMeshDestroy(EgMesh *mesh)
{
    RgDevice *device = egEngineGetDevice(mesh->engine);

    rgBufferDestroy(device, mesh->vertex_buffer);
    rgBufferDestroy(device, mesh->index_buffer);

    egFree(mesh->allocator, mesh);
}

RgBuffer *egMeshGetVertexBuffer(EgMesh* mesh)
{
    return mesh->vertex_buffer;
}

RgBuffer *egMeshGetIndexBuffer(EgMesh* mesh)
{
    return mesh->index_buffer;
}

uint32_t egMeshGetIndexCount(EgMesh* mesh)
{
    return mesh->index_count;
}

#include "model_asset.h"

#include <assert.h>
#include <rg.h>
#include "math.h"
#include "array.hpp"
#include "allocator.h"
#include "platform.h"
#include "engine.h"
#include "mesh.h"
#include "uniform_arena.h"

struct MaterialUniform
{
    Vec4 base_color;
    Vec4 emissive;
    float metallic;
    float roughness;
    uint32_t is_normal_mapped;
};

enum ModelType
{
    MODEL_FROM_MESH,
    MODEL_FROM_GLTF,
};

struct Material
{
    MaterialUniform uniform;

    RgImage *albedo_image;
    RgSampler *albedo_sampler;

    RgImage *normal_image;
    RgSampler *normal_sampler;

    RgImage *metallic_roughness_image;
    RgSampler *metallic_roughness_sampler;

    RgImage *occlusion_image;
    RgSampler *occlusion_sampler;

    RgImage *emissive_image;
    RgSampler *emissive_sampler;

    RgDescriptorSet *descriptor_set;
};

struct Primitive
{
    uint32_t first_index;
    uint32_t index_count;
    uint32_t vertex_count;
    int32_t material_index = -1;
    bool has_indices;
    bool is_normal_mapped;
};

struct ModelMesh
{
    Array<Primitive> primitives;
};

struct Node
{
    int64_t parent_index = -1;
    Array<uint32_t> children_indices;

    Mat4 matrix = Mat4Diagonal(1.0f);
    int64_t mesh_index = -1;

    Vec3 translation = V3(0.0, 0.0, 0.0);
    Vec3 scale = V3(1.0, 1.0, 1.0);
    Quat rotation = {0.0, 0.0, 0.0, 1.0};
};

struct ModelAsset
{
    Engine *engine;
    Allocator *allocator;
    UniformArena *uniform_arena;

    ModelType type;

    RgBuffer *vertex_buffer;
    RgBuffer *index_buffer;

    Array<Node> nodes;
    Array<uint32_t> root_nodes;
    Array<ModelMesh> meshes;
    Array<Material> materials;
    Array<RgImage*> images;
    Array<RgSampler*> samplers;
};

static Material MaterialDefault(Engine *engine)
{
    Material material = {};
    RgImage *white_image = EngineGetWhiteImage(engine);
    RgImage *black_image = EngineGetBlackImage(engine);
    RgSampler *default_sampler = EngineGetDefaultSampler(engine);

    material.albedo_image = white_image;
    material.normal_image = white_image;
    material.metallic_roughness_image = white_image;
    material.occlusion_image = white_image;
    material.emissive_image = black_image;

    material.albedo_sampler = default_sampler;
    material.normal_sampler = default_sampler;
    material.metallic_roughness_sampler = default_sampler;
    material.occlusion_sampler = default_sampler;
    material.emissive_sampler = default_sampler;
    return material;
}

ModelAsset *ModelAssetFromGltf(
        Allocator *allocator,
        Engine *engine,
        UniformArena *uniform_arena,
        const uint8_t *data,
        size_t size)
{
    ModelAsset *model = (ModelAsset*)Allocate(allocator, sizeof(ModelAsset));
    *model = {};

    model->allocator = allocator;
    model->engine = engine;
    model->uniform_arena = uniform_arena;
    model->type = MODEL_FROM_GLTF;

    (void)data;
    (void)size;

    assert(0);
    return model;
}

ModelAsset *ModelAssetFromMesh(
        Allocator *allocator,
        Engine *engine,
        UniformArena *uniform_arena,
        Mesh *mesh)
{
    ModelAsset *model = (ModelAsset*)Allocate(allocator, sizeof(ModelAsset));
    *model = {};

    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

    model->allocator = allocator;
    model->engine = engine;
    model->uniform_arena = uniform_arena;
    model->type = MODEL_FROM_MESH;

    model->vertex_buffer = MeshGetVertexBuffer(mesh);
    model->index_buffer = MeshGetIndexBuffer(mesh);

    model->nodes = Array<Node>::create(allocator);
    model->root_nodes = Array<uint32_t>::create(allocator);
    model->meshes = Array<ModelMesh>::create(allocator);
    model->materials = Array<Material>::create(allocator);
    model->images = Array<RgImage*>::create(allocator);
    model->samplers = Array<RgSampler*>::create(allocator);

    model->materials.push_back(MaterialDefault(engine));

    Primitive primitive = {};
    primitive.first_index = 0;
    primitive.index_count = MeshGetIndexCount(mesh);
    primitive.material_index = 0;
    primitive.has_indices = true;
    primitive.is_normal_mapped = false;

    ModelMesh model_mesh = {};
    model_mesh.primitives = Array<Primitive>::create(allocator);
    model_mesh.primitives.push_back(primitive);

    model->meshes.push_back(model_mesh);

    Node node = {};
    node.matrix = Mat4Diagonal(1.0f);
    node.mesh_index = 0;

    model->nodes.push_back(node);

    for (uint32_t i = 0; i < model->nodes.length; ++i)
    {
        Node *node = &model->nodes[i];
        if (node->parent_index == -1)
        {
            model->root_nodes.push_back(i);
        }
    }

    RgDescriptorSetLayout *material_set_layout =
        EngineGetSetLayout(engine, BIND_GROUP_MODEL);

    for (Material &material : model->materials)
    {
        RgDescriptorSetEntry entries[8] = {};
        entries[0].binding = 0;
        entries[0].buffer = UniformArenaGetBuffer(uniform_arena);

        entries[1].binding = 1;
        entries[1].buffer = UniformArenaGetBuffer(uniform_arena);

        entries[2].binding = 2;
        entries[2].sampler = material.albedo_sampler;

        entries[3].binding = 3;
        entries[3].image = material.albedo_image;

        entries[4].binding = 4;
        entries[4].image = material.normal_image;

        entries[5].binding = 5;
        entries[5].image = material.metallic_roughness_image;

        entries[6].binding = 6;
        entries[6].image = material.occlusion_image;

        entries[7].binding = 7;
        entries[7].image = material.emissive_image;

        RgDescriptorSetInfo info = {};
        info.layout = material_set_layout;
        info.entries = entries;
        info.entry_count = sizeof(entries)/sizeof(entries[0]);
        material.descriptor_set = rgDescriptorSetCreate(device, &info);
    }

    return model;
}

void ModelAssetDestroy(ModelAsset *model)
{
    Platform *platform = EngineGetPlatform(model->engine);
    RgDevice *device = PlatformGetDevice(platform);

    switch (model->type)
    {
    case MODEL_FROM_MESH:
    {
        break;
    }
    case MODEL_FROM_GLTF:
    {
        rgBufferDestroy(device, model->vertex_buffer);
        rgBufferDestroy(device, model->index_buffer);
        break;
    }
    }

    for (Material &material : model->materials)
    {
        rgDescriptorSetDestroy(device, material.descriptor_set);
    }

    for (Node &node : model->nodes)
    {
        node.children_indices.free();
    }

    for (ModelMesh &mesh : model->meshes)
    {
        mesh.primitives.free();
    }

    model->nodes.free();
    model->root_nodes.free();
    model->meshes.free();
    model->materials.free();
    model->images.free();
    model->samplers.free();

    Free(model->allocator, model);
}

static void NodeRender(
        ModelAsset *model,
        Node *node,
        RgCmdBuffer *cmd_buffer,
        Mat4 *transform)
{
    (void)node;
    (void)cmd_buffer;

    if (node->mesh_index != -1)
    {
        ModelMesh *mesh = &model->meshes[node->mesh_index];

        for (Primitive &primitive : mesh->primitives)
        {
            Material *material = &model->materials[primitive.material_index];

            uint32_t model_offset = 0;
            void *model_buffer = UniformArenaUse(
                    model->uniform_arena, &model_offset, sizeof(Mat4));
            memcpy(model_buffer, transform, sizeof(Mat4));

            uint32_t material_offset = 0;
            void *material_buffer = UniformArenaUse(
                    model->uniform_arena, &material_offset, sizeof(MaterialUniform));
            memcpy(material_buffer, &material->uniform, sizeof(MaterialUniform));

            uint32_t dynamic_offsets[] = {
                model_offset,
                material_offset,
            };

            rgCmdBindDescriptorSet(
                    cmd_buffer, 1, material->descriptor_set, 2, dynamic_offsets);

            if (primitive.has_indices)
            {
                rgCmdDrawIndexed(
                        cmd_buffer, primitive.index_count, 1, primitive.first_index, 0, 0);
            }
            else
            {
                rgCmdDraw(cmd_buffer, primitive.vertex_count, 1, 0, 0);
            }
        }
    }

    for (uint32_t &index : node->children_indices)
    {
        Node *child = &model->nodes[index];
        NodeRender(model, child, cmd_buffer, transform);
    }
}

void ModelAssetRender(ModelAsset *model, RgCmdBuffer *cmd_buffer, Mat4 *transform)
{
    assert(transform);
    rgCmdBindVertexBuffer(cmd_buffer, model->vertex_buffer, 0);
    rgCmdBindIndexBuffer(cmd_buffer, model->index_buffer, 0, RG_INDEX_TYPE_UINT32);
    for (Node &node : model->nodes)
    {
        NodeRender(model, &node, cmd_buffer, transform);
    }
}

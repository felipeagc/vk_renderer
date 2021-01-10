#include "model.h"

#include <assert.h>
#include <rg.h>
#include "math.h"
#include "array.h"
#include "allocator.h"
#include "platform.h"
#include "mesh.h"

struct ModelUniform
{
    Mat4 transform;
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
    Vec4 base_color;
    Vec4 emissive;
    float metallic;
    float roughness;
    uint32_t is_normal_mapped;

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

struct Model
{
    Platform *platform;
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

static Material MaterialDefault(Platform *platform)
{
    Material material = {};
    RgImage *white_image = PlatformGetWhiteImage(platform);
    RgImage *black_image = PlatformGetBlackImage(platform);
    RgSampler *default_sampler = PlatformGetDefaultSampler(platform);

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

Model *ModelFromGltf(
        Allocator *allocator,
        Platform *platform,
        UniformArena *uniform_arena,
        const uint8_t *data,
        size_t size)
{
    Model *model = (Model*)Allocate(allocator, sizeof(Model));
    *model = {};

    model->allocator = allocator;
    model->platform = platform;
    model->uniform_arena = uniform_arena;
    model->type = MODEL_FROM_GLTF;

    (void)data;
    (void)size;

    assert(0);
    return model;
}

Model *ModelFromMesh(
        Allocator *allocator,
        Platform *platform,
        UniformArena *uniform_arena,
        Mesh *mesh)
{
    Model *model = (Model*)Allocate(allocator, sizeof(Model));
    *model = {};

    model->allocator = allocator;
    model->platform = platform;
    model->uniform_arena = uniform_arena;
    model->type = MODEL_FROM_MESH;

    model->vertex_buffer = MeshGetVertexBuffer(mesh);
    model->index_buffer = MeshGetIndexBuffer(mesh);

    model->nodes = Array<Node>::with_allocator(allocator);
    model->root_nodes = Array<uint32_t>::with_allocator(allocator);
    model->meshes = Array<ModelMesh>::with_allocator(allocator);
    model->materials = Array<Material>::with_allocator(allocator);
    model->images = Array<RgImage*>::with_allocator(allocator);
    model->samplers = Array<RgSampler*>::with_allocator(allocator);

    model->materials.push_back(MaterialDefault(platform));

    Primitive primitive = {};
    primitive.first_index = 0;
    primitive.index_count = MeshGetIndexCount(mesh);
    primitive.material_index = 0;
    primitive.has_indices = true;
    primitive.is_normal_mapped = false;

    ModelMesh model_mesh = {};
    model_mesh.primitives = Array<Primitive>::with_allocator(allocator);
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

    /* for (Material &material : model->materials) */
    /* { */
    /*     material.descriptor_set = rgPipelineCreateDescriptorSet(); */
    /* } */

    return model;
}

void ModelDestroy(Model *model)
{
    RgDevice *device = PlatformGetDevice(model->platform);

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
        Model *model,
        Node *node,
        RgCmdBuffer *cmd_buffer,
        Mat4 *transform)
{
    (void)node;
    (void)cmd_buffer;
    (void)transform;

    if (node->mesh_index != -1)
    {
        ModelMesh *mesh = &model->meshes[node->mesh_index];

        for (Primitive &primitive : mesh->primitives)
        {
            Material *material = &model->materials[primitive.material_index];

            ModelUniform uniform = {};
            uniform.transform = node->matrix;
            if (transform)
            {
                uniform.transform = (*transform) * node->matrix;
            }
            uniform.base_color = material->base_color;
            uniform.emissive = material->emissive;
            uniform.metallic = material->metallic;
            uniform.roughness = material->roughness;
            uniform.is_normal_mapped = material->is_normal_mapped;

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

void ModelRender(Model *model, RgCmdBuffer *cmd_buffer, Mat4 *transform)
{
    rgCmdBindVertexBuffer(cmd_buffer, model->vertex_buffer, 0);
    rgCmdBindIndexBuffer(cmd_buffer, model->index_buffer, 0, RG_INDEX_TYPE_UINT32);
    for (Node &node : model->nodes)
    {
        NodeRender(model, &node, cmd_buffer, transform);
    }
}

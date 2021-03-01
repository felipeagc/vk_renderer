#include "model_asset.h"

#include <assert.h>
#include <stdio.h>
#include <rg.h>
#include "math.h"
#include "array.hpp"
#include "allocator.h"
#include "platform.h"
#include "engine.h"
#include "mesh.h"
#include "buffer_pool.h"
#include "camera.h"

struct ModelManager
{
	Allocator *allocator;
	Engine *engine;

	BufferPool *camera_buffer_pool;
	BufferPool *model_buffer_pool;
	BufferPool *material_buffer_pool;

	uint32_t current_camera_index;
};

struct ModelUniform
{
	Mat4 transform;
};

struct MaterialUniform
{
    Vec4 base_color;
    Vec4 emissive;

    float metallic;
    float roughness;
    uint32_t is_normal_mapped;

	uint32_t sampler_index;
	uint32_t albedo_image_index;
	uint32_t normal_image_index;
	uint32_t metallic_roughness_image_index;
	uint32_t occlusion_image_index;
	uint32_t emissive_image_index;
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

    SamplerHandle sampler;

    ImageHandle albedo_image;
    ImageHandle normal_image;
    ImageHandle metallic_roughness_image;
    ImageHandle occlusion_image;
    ImageHandle emissive_image;
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
	ModelManager *manager;

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

extern "C" ModelManager *ModelManagerCreate(
		Allocator *allocator, Engine *engine, size_t model_limit, size_t material_limit)
{
	ModelManager *manager = (ModelManager*)Allocate(allocator, sizeof(*manager));
	*manager = {};

	manager->allocator = allocator;
	manager->engine = engine;
	manager->camera_buffer_pool =
		BufferPoolCreate(manager->allocator, engine, sizeof(CameraUniform), 16);
	manager->model_buffer_pool =
		BufferPoolCreate(manager->allocator, engine, sizeof(ModelUniform), model_limit);
	manager->material_buffer_pool =
		BufferPoolCreate(manager->allocator, engine, sizeof(MaterialUniform), material_limit);

	return manager;
}

extern "C" void ModelManagerDestroy(ModelManager *manager)
{
	BufferPoolDestroy(manager->camera_buffer_pool);
	BufferPoolDestroy(manager->model_buffer_pool);
	BufferPoolDestroy(manager->material_buffer_pool);

	Free(manager->allocator, manager);
}

extern "C" void ModelManagerBeginFrame(ModelManager *manager, CameraUniform *camera_uniform)
{
	BufferPoolReset(manager->camera_buffer_pool);
	BufferPoolReset(manager->model_buffer_pool);
	BufferPoolReset(manager->material_buffer_pool);

	manager->current_camera_index =
		BufferPoolAllocateItem(manager->camera_buffer_pool, sizeof(*camera_uniform), camera_uniform);
}

static Material MaterialDefault(Engine *engine)
{
    Material material = {};
    ImageHandle white_image = EngineGetWhiteImage(engine);
    ImageHandle black_image = EngineGetBlackImage(engine);
    SamplerHandle default_sampler = EngineGetDefaultSampler(engine);

    material.albedo_image = white_image;
    material.normal_image = white_image;
    material.metallic_roughness_image = white_image;
    material.occlusion_image = white_image;
    material.emissive_image = black_image;

    material.sampler = default_sampler;

    return material;
}

extern "C" ModelAsset *ModelAssetFromGltf(
        ModelManager *manager,
        const uint8_t *data,
        size_t size)
{
	Allocator *allocator = manager->allocator;

    ModelAsset *model = (ModelAsset*)Allocate(allocator, sizeof(ModelAsset));
    *model = {};

    model->manager = manager;
    model->type = MODEL_FROM_GLTF;

    (void)data;
    (void)size;

    assert(0);
    return model;
}

extern "C" ModelAsset *ModelAssetFromMesh(
        ModelManager *manager,
        Mesh *mesh)
{
	Allocator *allocator = manager->allocator;
	Engine *engine = manager->engine;

    ModelAsset *model = (ModelAsset*)Allocate(allocator, sizeof(ModelAsset));
    *model = {};

    model->manager = manager;
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

    return model;
}

extern "C" void ModelAssetDestroy(ModelAsset *model)
{
    Platform *platform = EngineGetPlatform(model->manager->engine);
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

    Free(model->manager->allocator, model);
}

static void NodeRender(
        ModelAsset *model,
        Node *node,
        RgCmdBuffer *cmd_buffer,
        Mat4 *transform)
{
    (void)node;
    (void)cmd_buffer;

	ModelUniform model_uniform = {};
	model_uniform.transform = *transform;

    if (node->mesh_index != -1)
    {
        ModelMesh *mesh = &model->meshes[node->mesh_index];

        for (Primitive &primitive : mesh->primitives)
        {
            Material *material = &model->materials[primitive.material_index];

			uint32_t model_index = BufferPoolAllocateItem(
					model->manager->model_buffer_pool,
					sizeof(ModelUniform),
					&model_uniform);

			MaterialUniform material_uniform = {};
			material_uniform.base_color = material->base_color;
			material_uniform.emissive = material->emissive;
			material_uniform.metallic = material->metallic;
			material_uniform.roughness = material->roughness;
			material_uniform.is_normal_mapped = material->is_normal_mapped;

			material_uniform.sampler_index = material->sampler.index;
			material_uniform.albedo_image_index = material->albedo_image.index;
			material_uniform.normal_image_index = material->normal_image.index;
			material_uniform.metallic_roughness_image_index = material->metallic_roughness_image.index;
			material_uniform.occlusion_image_index = material->occlusion_image.index;
			material_uniform.emissive_image_index = material->emissive_image.index;

			uint32_t material_index = BufferPoolAllocateItem(
					model->manager->material_buffer_pool,
					sizeof(MaterialUniform),
					&material_uniform);

			struct {
				uint32_t camera_buffer_index;
				uint32_t camera_index;

				uint32_t model_buffer_index;
				uint32_t model_index;

				uint32_t material_buffer_index;
				uint32_t material_index;
			} pc;

			pc.camera_buffer_index = BufferPoolGetBufferIndex(model->manager->camera_buffer_pool);
			pc.camera_index = model->manager->current_camera_index;

			pc.model_buffer_index = BufferPoolGetBufferIndex(model->manager->model_buffer_pool);
			pc.model_index = model_index;

			pc.material_buffer_index = BufferPoolGetBufferIndex(model->manager->material_buffer_pool);
			pc.material_index = material_index;

			rgCmdPushConstants(cmd_buffer, 0, sizeof(pc), &pc);

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

extern "C" void ModelAssetRender(ModelAsset *model, RgCmdBuffer *cmd_buffer, Mat4 *transform)
{
    assert(transform);
    rgCmdBindVertexBuffer(cmd_buffer, model->vertex_buffer, 0);
    rgCmdBindIndexBuffer(cmd_buffer, model->index_buffer, 0, RG_INDEX_TYPE_UINT32);
    for (Node &node : model->nodes)
    {
        NodeRender(model, &node, cmd_buffer, transform);
    }
}

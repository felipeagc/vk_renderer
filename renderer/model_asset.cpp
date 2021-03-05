#include "model_asset.h"

#include <stdio.h>
#include <stdalign.h>
#include <rg.h>
#include <cgltf.h>
#include <stb_image.h>
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
    float4x4 transform;
};

struct MaterialUniform
{
    float4 base_color;
    float4 emissive;

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

enum ModelType {
    MODEL_FROM_MESH,
    MODEL_FROM_GLTF,
};

struct Material
{
    float4 base_color;
    float4 emissive;

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

    float4x4 matrix = eg_float4x4_diagonal(1.0f);
    int64_t mesh_index = -1;

    float3 translation = V3(0.0, 0.0, 0.0);
    float3 scale = V3(1.0, 1.0, 1.0);
    quat128 rotation = {0.0, 0.0, 0.0, 1.0};
};

struct ModelAsset
{
    ModelManager *manager;

    ModelType type;

    RgBuffer *vertex_buffer;
    RgBuffer *index_buffer;

    Array<Node> nodes;
    Array<size_t> root_nodes;
    Array<ModelMesh> meshes;
    Array<Material> materials;
    Array<ImageHandle> images;
    Array<SamplerHandle> samplers;
};

static float4x4 NodeLocalMatrix(Node *node)
{
    float4x4 result = node->matrix;

    eg_float4x4_scale(&result, node->scale);

    float4x4 rot_mat = eg_quat_to_matrix(node->rotation);
    result = eg_float4x4_mul(&result, &rot_mat);

    eg_float4x4_translate(&result, node->translation);

    return result;
}

static float4x4 NodeResolveMatrix(Node *node, ModelAsset *model)
{
    float4x4 m = NodeLocalMatrix(node);
    int32_t p = node->parent_index;
    while (p != -1)
    {
        float4x4 parent_local_mat = NodeLocalMatrix(&model->nodes[p]);
        m = eg_float4x4_mul(&m, &parent_local_mat);
        p = model->nodes[p].parent_index;
    }

    return m;
}

extern "C" ModelManager *ModelManagerCreate(
    Allocator *allocator, Engine *engine, size_t model_limit, size_t material_limit)
{
    ModelManager *manager = (ModelManager *)Allocate(allocator, sizeof(*manager));
    *manager = {};

    manager->allocator = allocator;
    manager->engine = engine;
    manager->camera_buffer_pool =
        BufferPoolCreate(manager->allocator, engine, sizeof(CameraUniform), 16);
    manager->model_buffer_pool =
        BufferPoolCreate(manager->allocator, engine, sizeof(ModelUniform), model_limit);
    manager->material_buffer_pool = BufferPoolCreate(
        manager->allocator, engine, sizeof(MaterialUniform), material_limit);

    return manager;
}

extern "C" void ModelManagerDestroy(ModelManager *manager)
{
    BufferPoolDestroy(manager->camera_buffer_pool);
    BufferPoolDestroy(manager->model_buffer_pool);
    BufferPoolDestroy(manager->material_buffer_pool);

    Free(manager->allocator, manager);
}

extern "C" void
ModelManagerBeginFrame(ModelManager *manager, CameraUniform *camera_uniform)
{
    BufferPoolReset(manager->camera_buffer_pool);
    BufferPoolReset(manager->model_buffer_pool);
    BufferPoolReset(manager->material_buffer_pool);

    manager->current_camera_index = BufferPoolAllocateItem(
        manager->camera_buffer_pool, sizeof(*camera_uniform), camera_uniform);
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

extern "C" ModelAsset *
ModelAssetFromGltf(ModelManager *manager, const uint8_t *data, size_t size)
{
    Engine *engine = manager->engine;
    Allocator *allocator = manager->allocator;
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);
    RgCmdPool *transfer_cmd_pool = EngineGetTransferCmdPool(engine);

    ModelAsset *model = (ModelAsset *)Allocate(allocator, sizeof(ModelAsset));
    *model = {};

    model->manager = manager;
    model->type = MODEL_FROM_GLTF;

    model->nodes = Array<Node>::create(allocator);
    model->root_nodes = Array<size_t>::create(allocator);
    model->meshes = Array<ModelMesh>::create(allocator);
    model->materials = Array<Material>::create(allocator);
    model->images = Array<ImageHandle>::create(allocator);
    model->samplers = Array<SamplerHandle>::create(allocator);

    cgltf_options gltf_options = {};
    gltf_options.type = cgltf_file_type_glb;
    cgltf_data *gltf_data = NULL;
    cgltf_result result = cgltf_parse(&gltf_options, data, size, &gltf_data);
    if (result != cgltf_result_success)
    {
        cgltf_free(gltf_data);
        return NULL;
    }

    result = cgltf_load_buffers(&gltf_options, gltf_data, NULL);
    if (result != cgltf_result_success)
    {
        cgltf_free(gltf_data);
        return NULL;
    }

    model->images.resize(gltf_data->images_count);
    for (uint32_t i = 0; i < gltf_data->images_count; ++i)
    {
        cgltf_image *gltf_image = &gltf_data->images[i];
        const char *mime_type = gltf_image->mime_type;

        if (strcmp(mime_type, "image/png") == 0 || strcmp(mime_type, "image/jpeg") == 0)
        {
            cgltf_buffer *buffer = gltf_image->buffer_view->buffer;

            size_t size = gltf_image->buffer_view->size;
            uint8_t *buffer_data =
                (uint8_t *)buffer->data + gltf_image->buffer_view->offset;

            int32_t width = 0;
            int32_t height = 0;
            int32_t n_channels = 0;

            uint8_t *image_data = stbi_load_from_memory(
                buffer_data, (int)size, &width, &height, &n_channels, 4);
            if (!image_data)
            {
                return NULL;
            }

            EG_ASSERT(width > 0 && height > 0 && n_channels > 0);

            /* uint32_t mip_count = (uint32_t)floor(log2((float)(width > height ? width :
             * height))) + 1; */

            RgImageInfo image_info = {};
            image_info.format = RG_FORMAT_RGBA8_UNORM;
            image_info.extent = {(uint32_t)width, (uint32_t)height, 1};
            image_info.aspect = RG_IMAGE_ASPECT_COLOR;
            image_info.layer_count = 1;
            image_info.sample_count = 1;
            image_info.mip_count = 1;
            image_info.usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_TRANSFER_DST;

            model->images[i] = EngineAllocateImageHandle(engine, &image_info);

            RgImageCopy image_copy = {};
            image_copy.array_layer = 0;
            image_copy.image = model->images[i].image;
            image_copy.mip_level = 0;
            image_copy.offset = RgOffset3D{0, 0, 0};
            rgImageUpload(
                device,
                transfer_cmd_pool,
                &image_copy,
                &image_info.extent,
                (size_t)(width * height * 4),
                image_data);

            stbi_image_free(image_data);
        }
        else
        {
            printf(
                "Unsupported image format for GLTF model: %s\n", gltf_image->mime_type);
            EG_ASSERT(0);
        }
    }

    model->samplers.resize(gltf_data->samplers_count);
    for (uint32_t i = 0; i < gltf_data->samplers_count; ++i)
    {
        cgltf_sampler gltf_sampler = gltf_data->samplers[i];

        RgSamplerInfo sampler_info = {};
        sampler_info.anisotropy = true;
        sampler_info.max_anisotropy = 16.0;
        sampler_info.mag_filter = RG_FILTER_LINEAR;
        sampler_info.min_filter = RG_FILTER_LINEAR;
        sampler_info.min_lod = 0.0f;
        sampler_info.max_lod = 1.0f;
        sampler_info.address_mode = RG_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.border_color = RG_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        switch (gltf_sampler.mag_filter)
        {
        case 0x2600: sampler_info.mag_filter = RG_FILTER_NEAREST; break;
        case 0x2601: sampler_info.mag_filter = RG_FILTER_LINEAR; break;
        default: break;
        }

        switch (gltf_sampler.min_filter)
        {
        case 0x2600: sampler_info.min_filter = RG_FILTER_NEAREST; break;
        case 0x2601: sampler_info.min_filter = RG_FILTER_LINEAR; break;
        default: break;
        }

        model->samplers[i] = EngineAllocateSamplerHandle(engine, &sampler_info);
    }

    model->materials.resize(gltf_data->materials_count);

    for (size_t i = 0; i < gltf_data->materials_count; ++i)
    {
        cgltf_material gltf_material = gltf_data->materials[i];
        EG_ASSERT(gltf_material.has_pbr_metallic_roughness);

        Material *mat = &model->materials[i];
        *mat = MaterialDefault(engine);

        if (model->samplers.length > 0)
        {
            mat->sampler = model->samplers[0];
        }

        cgltf_texture *gltf_albedo_texture =
            gltf_material.pbr_metallic_roughness.base_color_texture.texture;
        if (gltf_albedo_texture != NULL)
        {
            size_t image_index = (size_t)(gltf_albedo_texture->image - gltf_data->images);
            mat->albedo_image = model->images[image_index];
        }

        cgltf_texture *gltf_normal_texture = gltf_material.normal_texture.texture;
        if (gltf_normal_texture != NULL)
        {
            size_t image_index = (size_t)(gltf_normal_texture->image - gltf_data->images);
            mat->normal_image = model->images[image_index];
        }

        cgltf_texture *gltf_mr_texture =
            gltf_material.pbr_metallic_roughness.metallic_roughness_texture.texture;
        if (gltf_mr_texture != NULL)
        {
            size_t image_index = (size_t)(gltf_mr_texture->image - gltf_data->images);
            mat->metallic_roughness_image = model->images[image_index];
        }

        cgltf_texture *gltf_occlusion_texture = gltf_material.occlusion_texture.texture;
        if (gltf_occlusion_texture != NULL)
        {
            size_t image_index =
                (size_t)(gltf_occlusion_texture->image - gltf_data->images);
            mat->occlusion_image = model->images[image_index];
        }

        cgltf_texture *gltf_emissive_texture = gltf_material.emissive_texture.texture;
        if (gltf_emissive_texture != NULL)
        {
            size_t image_index =
                (size_t)(gltf_emissive_texture->image - gltf_data->images);
            mat->emissive_image = model->images[image_index];
        }
    }

    Array<Vertex> vertices = Array<Vertex>::create(allocator);
    Array<uint32_t> indices = Array<uint32_t>::create(allocator);

    model->meshes.resize(gltf_data->meshes_count);
    for (size_t i = 0; i < gltf_data->meshes_count; ++i)
    {
        Array<Primitive> primitives = Array<Primitive>::create(allocator);

        cgltf_mesh *gltf_mesh = &gltf_data->meshes[i];
        for (size_t j = 0; j < gltf_mesh->primitives_count; ++j)
        {
            cgltf_primitive *gltf_primitive = &gltf_mesh->primitives[j];

            size_t index_start = indices.length;
            size_t vertex_start = vertices.length;

            size_t index_count = 0;
            size_t vertex_count = 0;

            bool has_indices = gltf_primitive->indices != NULL;

            size_t pos_byte_stride = 0;
            uint8_t *pos_buffer = NULL;

            size_t normal_byte_stride = 0;
            uint8_t *normal_buffer = NULL;

            size_t tangent_byte_stride = 0;
            uint8_t *tangent_buffer = NULL;

            size_t uv0_byte_stride = 0;
            uint8_t *uv0_buffer = NULL;

            for (size_t k = 0; k < gltf_primitive->attributes_count; ++k)
            {
                switch (gltf_primitive->attributes[k].type)
                {
                case cgltf_attribute_type_position: {
                    cgltf_accessor *accessor = gltf_primitive->attributes[k].data;
                    cgltf_buffer_view *view = accessor->buffer_view;

                    pos_byte_stride = accessor->stride;
                    pos_buffer =
                        ((uint8_t *)view->buffer->data) + accessor->offset + view->offset;

                    vertex_count += accessor->count;

                    break;
                }
                case cgltf_attribute_type_normal: {
                    cgltf_accessor *accessor = gltf_primitive->attributes[k].data;
                    cgltf_buffer_view *view = accessor->buffer_view;

                    normal_byte_stride = accessor->stride;
                    normal_buffer =
                        ((uint8_t *)view->buffer->data) + accessor->offset + view->offset;

                    break;
                }
                case cgltf_attribute_type_tangent: {
                    cgltf_accessor *accessor = gltf_primitive->attributes[k].data;
                    cgltf_buffer_view *view = accessor->buffer_view;

                    tangent_byte_stride = accessor->stride;
                    tangent_buffer =
                        ((uint8_t *)view->buffer->data) + accessor->offset + view->offset;

                    break;
                }
                case cgltf_attribute_type_texcoord: {
                    cgltf_accessor *accessor = gltf_primitive->attributes[k].data;
                    cgltf_buffer_view *view = accessor->buffer_view;

                    uv0_byte_stride = accessor->stride;
                    uv0_buffer =
                        ((uint8_t *)view->buffer->data) + accessor->offset + view->offset;

                    break;
                }
                default: break;
                }
            }

            vertices.resize(vertices.length + vertex_count);

            Vertex *new_vertices = &vertices[vertices.length - vertex_count];

            for (size_t k = 0; k < vertex_count; ++k)
            {
                memcpy(
                    &new_vertices[k].pos,
                    &pos_buffer[k * pos_byte_stride],
                    sizeof(new_vertices->pos));
            }

            if (normal_buffer)
            {
                for (size_t k = 0; k < vertex_count; ++k)
                {
                    memcpy(
                        &new_vertices[k].normal,
                        &normal_buffer[k * normal_byte_stride],
                        sizeof(new_vertices->normal));
                }
            }

            if (tangent_buffer)
            {
                for (size_t k = 0; k < vertex_count; ++k)
                {
                    memcpy(
                        &new_vertices[k].tangent,
                        &tangent_buffer[k * tangent_byte_stride],
                        sizeof(new_vertices->tangent));
                }
            }

            if (uv0_buffer)
            {
                for (size_t k = 0; k < vertex_count; ++k)
                {
                    memcpy(
                        &new_vertices[k].uv,
                        &uv0_buffer[k * uv0_byte_stride],
                        sizeof(new_vertices->uv));
                }
            }

            if (has_indices)
            {
                cgltf_accessor *accessor = gltf_primitive->indices;
                cgltf_buffer_view *buffer_view = accessor->buffer_view;
                cgltf_buffer *buffer = buffer_view->buffer;

                index_count = accessor->count;

                indices.resize(indices.length + index_count);
                uint32_t *new_indices = &indices[indices.length - index_count];

                uint8_t *data_ptr =
                    ((uint8_t *)buffer->data) + accessor->offset + buffer_view->offset;

                switch (accessor->component_type)
                {
                case cgltf_component_type_r_32u: {
                    uint32_t *buf = (uint32_t *)data_ptr;
                    for (size_t k = 0; k < index_count; ++k)
                    {
                        new_indices[k] = buf[k] + vertex_start;
                    }
                    break;
                }
                case cgltf_component_type_r_16u: {
                    uint16_t *buf = (uint16_t *)data_ptr;
                    for (size_t k = 0; k < index_count; ++k)
                    {
                        new_indices[k] = (uint32_t)buf[k] + vertex_start;
                    }
                    break;
                }
                case cgltf_component_type_r_8u: {
                    uint8_t *buf = (uint8_t *)data_ptr;
                    for (size_t k = 0; k < index_count; ++k)
                    {
                        new_indices[k] = (uint32_t)buf[k] + vertex_start;
                    }
                    break;
                }
                default: EG_ASSERT(0); break;
                }
            }

            Primitive new_primitive = {
                .first_index = (uint32_t)index_start,
                .index_count = (uint32_t)index_count,
                .vertex_count = (uint32_t)vertex_count,
                .material_index = -1,
                .has_indices = has_indices,
                .is_normal_mapped = normal_buffer != NULL && tangent_buffer != NULL,
            };

            if (gltf_primitive->material)
            {
                new_primitive.material_index =
                    (size_t)(gltf_primitive->material - gltf_data->materials);
            }

            primitives.push_back(new_primitive);
        }

        model->meshes[i] = (ModelMesh){
            .primitives = primitives,
        };
    }

    size_t vertex_buffer_size = sizeof(Vertex) * vertices.length;
    size_t index_buffer_size = sizeof(uint32_t) * indices.length;
    EG_ASSERT(vertex_buffer_size > 0);

    RgBufferInfo vertex_buffer_info = {};
    vertex_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;
    vertex_buffer_info.usage = RG_BUFFER_USAGE_VERTEX | RG_BUFFER_USAGE_TRANSFER_DST;
    vertex_buffer_info.size = vertex_buffer_size;
    model->vertex_buffer = rgBufferCreate(device, &vertex_buffer_info);

    RgBufferInfo index_buffer_info = {};
    index_buffer_info.memory = RG_BUFFER_MEMORY_DEVICE;
    index_buffer_info.usage = RG_BUFFER_USAGE_INDEX | RG_BUFFER_USAGE_TRANSFER_DST;
    index_buffer_info.size = index_buffer_size;
    model->index_buffer = rgBufferCreate(device, &index_buffer_info);

    rgBufferUpload(
        device,
        transfer_cmd_pool,
        model->vertex_buffer,
        0,
        vertex_buffer_size,
        &vertices[0]);
    rgBufferUpload(
        device,
        transfer_cmd_pool,
        model->index_buffer,
        0,
        index_buffer_size,
        &indices[0]);

    model->nodes.resize(gltf_data->nodes_count);
    for (size_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node *gltf_node = &gltf_data->nodes[i];
        Node *node = &model->nodes[i];

        *node = (Node){
            .parent_index = -1,
            .children_indices = Array<uint32_t>::create(allocator),

            .matrix = eg_float4x4_diagonal(1.0f),
            .mesh_index = -1,

            .translation = V3(0.0, 0.0, 0.0),
            .scale = V3(1.0, 1.0, 1.0),
            .rotation = {0.0, 0.0, 0.0, 1.0},
        };

        if (gltf_node->has_translation)
        {
            node->translation.x = gltf_node->translation[0];
            node->translation.y = gltf_node->translation[1];
            node->translation.z = gltf_node->translation[2];
        }

        if (gltf_node->has_scale)
        {
            node->scale.x = gltf_node->scale[0];
            node->scale.y = gltf_node->scale[1];
            node->scale.z = gltf_node->scale[2];
        }

        if (gltf_node->has_rotation)
        {
            node->rotation.x = gltf_node->rotation[0];
            node->rotation.y = gltf_node->rotation[1];
            node->rotation.z = gltf_node->rotation[2];
            node->rotation.w = gltf_node->rotation[3];
        }

        if (gltf_node->has_matrix)
        {
            memcpy(&node->matrix, gltf_node->rotation, sizeof(float) * 16);
        }

        if (gltf_node->mesh)
        {
            size_t mesh_index = (size_t)(gltf_node->mesh - gltf_data->meshes);
            node->mesh_index = mesh_index;
        }

        if (gltf_node->parent)
        {
            size_t parent_index = (size_t)(gltf_node->parent - gltf_data->nodes);
            node->parent_index = parent_index;
        }
    }

    // Add children / root nodes
    for (size_t i = 0; i < model->nodes.length; ++i)
    {
        if (model->nodes[i].parent_index == -1)
        {
            model->root_nodes.push_back(i);
        }
        else
        {
            model->nodes[model->nodes[i].parent_index].children_indices.push_back(i);
        }
    }

    for (size_t i = 0; i < model->nodes.length; ++i)
    {
        model->nodes[i].matrix = NodeResolveMatrix(&model->nodes[i], model);
    }

    indices.free();
    vertices.free();

    cgltf_free(gltf_data);

    return model;
}

extern "C" ModelAsset *ModelAssetFromMesh(ModelManager *manager, Mesh *mesh)
{
    Allocator *allocator = manager->allocator;
    Engine *engine = manager->engine;

    ModelAsset *model = (ModelAsset *)Allocate(allocator, sizeof(ModelAsset));
    *model = {};

    model->manager = manager;
    model->type = MODEL_FROM_MESH;

    model->nodes = Array<Node>::create(allocator);
    model->root_nodes = Array<size_t>::create(allocator);
    model->meshes = Array<ModelMesh>::create(allocator);
    model->materials = Array<Material>::create(allocator);
    model->images = Array<ImageHandle>::create(allocator);
    model->samplers = Array<SamplerHandle>::create(allocator);

    model->vertex_buffer = MeshGetVertexBuffer(mesh);
    model->index_buffer = MeshGetIndexBuffer(mesh);

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
    node.matrix = eg_float4x4_diagonal(1.0f);
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
    Engine *engine = model->manager->engine;
    Platform *platform = EngineGetPlatform(engine);
    RgDevice *device = PlatformGetDevice(platform);

    switch (model->type)
    {
    case MODEL_FROM_MESH: {
        break;
    }
    case MODEL_FROM_GLTF: {
        for (SamplerHandle &sampler : model->samplers)
        {
            EngineFreeSamplerHandle(engine, &sampler);
        }

        for (ImageHandle &image : model->images)
        {
            EngineFreeImageHandle(engine, &image);
        }

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

static void
NodeRender(ModelAsset *model, Node *node, RgCmdBuffer *cmd_buffer, float4x4 *transform)
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
                model->manager->model_buffer_pool, sizeof(ModelUniform), &model_uniform);

            MaterialUniform material_uniform = {};
            material_uniform.base_color = material->base_color;
            material_uniform.emissive = material->emissive;
            material_uniform.metallic = material->metallic;
            material_uniform.roughness = material->roughness;
            material_uniform.is_normal_mapped = material->is_normal_mapped;

            material_uniform.sampler_index = material->sampler.index;
            material_uniform.albedo_image_index = material->albedo_image.index;
            material_uniform.normal_image_index = material->normal_image.index;
            material_uniform.metallic_roughness_image_index =
                material->metallic_roughness_image.index;
            material_uniform.occlusion_image_index = material->occlusion_image.index;
            material_uniform.emissive_image_index = material->emissive_image.index;

            uint32_t material_index = BufferPoolAllocateItem(
                model->manager->material_buffer_pool,
                sizeof(MaterialUniform),
                &material_uniform);

            struct
            {
                uint32_t camera_buffer_index;
                uint32_t camera_index;

                uint32_t model_buffer_index;
                uint32_t model_index;

                uint32_t material_buffer_index;
                uint32_t material_index;
            } pc;

            pc.camera_buffer_index =
                BufferPoolGetBufferIndex(model->manager->camera_buffer_pool);
            pc.camera_index = model->manager->current_camera_index;

            pc.model_buffer_index =
                BufferPoolGetBufferIndex(model->manager->model_buffer_pool);
            pc.model_index = model_index;

            pc.material_buffer_index =
                BufferPoolGetBufferIndex(model->manager->material_buffer_pool);
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

extern "C" void
ModelAssetRender(ModelAsset *model, RgCmdBuffer *cmd_buffer, float4x4 *transform)
{
    EG_ASSERT(transform);
    rgCmdBindVertexBuffer(cmd_buffer, model->vertex_buffer, 0);
    rgCmdBindIndexBuffer(cmd_buffer, model->index_buffer, 0, RG_INDEX_TYPE_UINT32);
    for (Node &node : model->nodes)
    {
        NodeRender(model, &node, cmd_buffer, transform);
    }
}

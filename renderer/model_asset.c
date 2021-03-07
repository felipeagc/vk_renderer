#include "model_asset.h"

#include <stdio.h>
#include <stdalign.h>
#include <rg.h>
#include <cgltf.h>
#include <stb_image.h>
#include "math.h"
#include "array.h"
#include "allocator.h"
#include "engine.h"
#include "mesh.h"
#include "buffer_pool.h"
#include "camera.h"

struct EgModelManager
{
    EgAllocator *allocator;
    EgEngine *engine;

    EgBufferPool *camera_buffer_pool;
    EgBufferPool *model_buffer_pool;
    EgBufferPool *material_buffer_pool;

    uint32_t current_camera_index;
};

typedef struct ModelUniform
{
    float4x4 transform;
} ModelUniform;

typedef struct MaterialUniform
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
    uint32_t brdf_image_index;
} MaterialUniform;

typedef enum ModelType {
    MODEL_FROM_MESH,
    MODEL_FROM_GLTF,
} ModelType;

typedef struct Material
{
    float4 base_color;
    float4 emissive;

    float metallic;
    float roughness;
    uint32_t is_normal_mapped;

    EgSampler sampler;

    EgImage albedo_image;
    EgImage normal_image;
    EgImage metallic_roughness_image;
    EgImage occlusion_image;
    EgImage emissive_image;
} Material;

typedef struct Primitive
{
    uint32_t first_index;
    uint32_t index_count;
    uint32_t vertex_count;
    int32_t material_index;
    bool has_indices;
    bool is_normal_mapped;
} Primitive;

typedef struct ModelMesh
{
    EgArray(Primitive) primitives;
} ModelMesh;

typedef struct Node
{
    int64_t parent_index;
    EgArray(size_t) children_indices;

    float4x4 matrix;
    float4x4 resolved_matrix;
    int64_t mesh_index;

    float3 translation;
    float3 scale;
    quat128 rotation;
} Node;

typedef struct EgModelAsset
{
    EgModelManager *manager;

    ModelType type;

    RgBuffer *vertex_buffer;
    RgBuffer *index_buffer;

    EgArray(Node) nodes;
    EgArray(size_t) root_nodes;
    EgArray(ModelMesh) meshes;
    EgArray(Material) materials;
    EgArray(EgImage) images;
    EgArray(EgSampler) samplers;
} EgModelAsset;

static float4x4 NodeLocalMatrix(Node *node)
{
    float4x4 result = egFloat4x4Diagonal(1.0f);

    egFloat4x4Translate(&result, node->translation);

    float3 axis;
    float angle;
    egQuatToAxisAngle(node->rotation, &axis, &angle);
    egFloat4x4Rotate(&result, angle, axis);

    egFloat4x4Scale(&result, node->scale);

    return egFloat4x4Mul(&result, &node->matrix);
}

static float4x4 NodeResolveMatrix(Node *node, EgModelAsset *model)
{
    float4x4 m = NodeLocalMatrix(node);
    int32_t p = node->parent_index;
    while (p != -1)
    {
        float4x4 parent_local_mat = NodeLocalMatrix(&model->nodes[p]);
        m = egFloat4x4Mul(&m, &parent_local_mat);
        p = model->nodes[p].parent_index;
    }

    return m;
}

EgModelManager *egModelManagerCreate(
    EgAllocator *allocator, EgEngine *engine, size_t model_limit, size_t material_limit)
{
    EgModelManager *manager = (EgModelManager *)egAllocate(allocator, sizeof(*manager));
    *manager = (EgModelManager){};

    manager->allocator = allocator;
    manager->engine = engine;
    manager->camera_buffer_pool =
        egBufferPoolCreate(manager->allocator, engine, sizeof(EgCameraUniform), 16);
    manager->model_buffer_pool =
        egBufferPoolCreate(manager->allocator, engine, sizeof(ModelUniform), model_limit);
    manager->material_buffer_pool = egBufferPoolCreate(
        manager->allocator, engine, sizeof(MaterialUniform), material_limit);

    return manager;
}

void egModelManagerDestroy(EgModelManager *manager)
{
    egBufferPoolDestroy(manager->camera_buffer_pool);
    egBufferPoolDestroy(manager->model_buffer_pool);
    egBufferPoolDestroy(manager->material_buffer_pool);

    egFree(manager->allocator, manager);
}

void
egModelManagerBeginFrame(EgModelManager *manager, EgCameraUniform *camera_uniform)
{
    egBufferPoolReset(manager->camera_buffer_pool);
    egBufferPoolReset(manager->model_buffer_pool);
    egBufferPoolReset(manager->material_buffer_pool);

    manager->current_camera_index = egBufferPoolAllocateItem(
        manager->camera_buffer_pool, sizeof(*camera_uniform), camera_uniform);
}

static Material MaterialDefault(EgEngine *engine)
{
    Material material = {};
    EgImage white_image = egEngineGetWhiteImage(engine);
    EgImage black_image = egEngineGetBlackImage(engine);
    EgSampler default_sampler = egEngineGetDefaultSampler(engine);

    material.albedo_image = white_image;
    material.normal_image = white_image;
    material.metallic_roughness_image = white_image;
    material.occlusion_image = white_image;
    material.emissive_image = black_image;

    material.sampler = default_sampler;

    return material;
}

EgModelAsset *
egModelAssetFromGltf(EgModelManager *manager, const uint8_t *data, size_t size)
{
    EgEngine *engine = manager->engine;
    EgAllocator *allocator = manager->allocator;
    RgDevice *device = egEngineGetDevice(engine);
    RgCmdPool *transfer_cmd_pool = egEngineGetTransferCmdPool(engine);

    EgModelAsset *model = (EgModelAsset *)egAllocate(allocator, sizeof(EgModelAsset));
    *model = (EgModelAsset){};

    model->manager = manager;
    model->type = MODEL_FROM_GLTF;

    model->nodes = egArrayCreate(allocator, Node);
    model->root_nodes = egArrayCreate(allocator, size_t);
    model->meshes = egArrayCreate(allocator, ModelMesh);
    model->materials = egArrayCreate(allocator, Material);
    model->images = egArrayCreate(allocator, EgImage);
    model->samplers = egArrayCreate(allocator, EgSampler);

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

    egArrayResize(&model->images, gltf_data->images_count);
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
            image_info.extent = (RgExtent3D){(uint32_t)width, (uint32_t)height, 1};
            image_info.aspect = RG_IMAGE_ASPECT_COLOR;
            image_info.layer_count = 1;
            image_info.sample_count = 1;
            image_info.mip_count = 1;
            image_info.usage = RG_IMAGE_USAGE_SAMPLED | RG_IMAGE_USAGE_TRANSFER_DST;

            model->images[i] = egEngineAllocateImage(engine, &image_info);

            RgImageCopy image_copy = {};
            image_copy.array_layer = 0;
            image_copy.image = model->images[i].image;
            image_copy.mip_level = 0;
            image_copy.offset = (RgOffset3D){0, 0, 0};
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

    egArrayResize(&model->samplers, gltf_data->samplers_count);
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

        model->samplers[i] = egEngineAllocateSampler(engine, &sampler_info);
    }

    egArrayResize(&model->materials, gltf_data->materials_count);

    for (size_t i = 0; i < gltf_data->materials_count; ++i)
    {
        cgltf_material gltf_material = gltf_data->materials[i];
        EG_ASSERT(gltf_material.has_pbr_metallic_roughness);

        Material *mat = &model->materials[i];
        *mat = MaterialDefault(engine);

        if (egArrayLength(model->samplers) > 0)
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

    EgArray(EgVertex) vertices = egArrayCreate(allocator, EgVertex);
    EgArray(uint32_t) indices = egArrayCreate(allocator, uint32_t);

    egArrayResize(&model->meshes, gltf_data->meshes_count);
    for (size_t i = 0; i < gltf_data->meshes_count; ++i)
    {
        EgArray(Primitive) primitives = egArrayCreate(allocator, Primitive);

        cgltf_mesh *gltf_mesh = &gltf_data->meshes[i];
        for (size_t j = 0; j < gltf_mesh->primitives_count; ++j)
        {
            cgltf_primitive *gltf_primitive = &gltf_mesh->primitives[j];

            size_t index_start = egArrayLength(indices);
            size_t vertex_start = egArrayLength(vertices);

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

            egArrayResize(&vertices, egArrayLength(vertices) + vertex_count);

            EgVertex *new_vertices = &vertices[egArrayLength(vertices) - vertex_count];

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

                egArrayResize(&indices, egArrayLength(indices) + index_count);
                uint32_t *new_indices = &indices[egArrayLength(indices) - index_count];

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
                .is_normal_mapped = ((normal_buffer != NULL) && (tangent_buffer != NULL)),
            };

            if (gltf_primitive->material)
            {
                new_primitive.material_index =
                    (size_t)(gltf_primitive->material - gltf_data->materials);
            }

            egArrayPush(&primitives, new_primitive);
        }

        model->meshes[i] = (ModelMesh){
            .primitives = primitives,
        };
    }

    size_t vertex_buffer_size = sizeof(EgVertex) * egArrayLength(vertices);
    size_t index_buffer_size = sizeof(uint32_t) * egArrayLength(indices);
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
        device, transfer_cmd_pool, model->vertex_buffer, 0, vertex_buffer_size, vertices);
    rgBufferUpload(
        device, transfer_cmd_pool, model->index_buffer, 0, index_buffer_size, indices);

    egArrayResize(&model->nodes, gltf_data->nodes_count);
    for (size_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node *gltf_node = &gltf_data->nodes[i];
        Node *node = &model->nodes[i];

        *node = (Node){
            .parent_index = -1,
            .children_indices = egArrayCreate(allocator, size_t),

            .matrix = egFloat4x4Diagonal(1.0f),
            .resolved_matrix = egFloat4x4Diagonal(1.0f),
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
    for (size_t i = 0; i < egArrayLength(model->nodes); ++i)
    {
        if (model->nodes[i].parent_index == -1)
        {
            egArrayPush(&model->root_nodes, i);
        }
        else
        {
            size_t parent_index = model->nodes[i].parent_index;
            Node *parent = &model->nodes[parent_index];
            egArrayPush(&parent->children_indices, i);
        }
    }

    for (size_t i = 0; i < egArrayLength(model->nodes); ++i)
    {
        model->nodes[i].resolved_matrix = NodeResolveMatrix(&model->nodes[i], model);
    }

    egArrayFree(&indices);
    egArrayFree(&vertices);

    cgltf_free(gltf_data);

    return model;
}

EgModelAsset *egModelAssetFromMesh(EgModelManager *manager, EgMesh *mesh)
{
    EgAllocator *allocator = manager->allocator;
    EgEngine *engine = manager->engine;

    EgModelAsset *model = (EgModelAsset *)egAllocate(allocator, sizeof(EgModelAsset));
    *model = (EgModelAsset){};

    model->manager = manager;
    model->type = MODEL_FROM_MESH;

    model->nodes = egArrayCreate(allocator, Node);
    model->root_nodes = egArrayCreate(allocator, size_t);
    model->meshes = egArrayCreate(allocator, ModelMesh);
    model->materials = egArrayCreate(allocator, Material);
    model->images = egArrayCreate(allocator, EgImage);
    model->samplers = egArrayCreate(allocator, EgSampler);

    model->vertex_buffer = egMeshGetVertexBuffer(mesh);
    model->index_buffer = egMeshGetIndexBuffer(mesh);

    egArrayPush(&model->materials, MaterialDefault(engine));

    Primitive primitive = {};
    primitive.first_index = 0;
    primitive.index_count = egMeshGetIndexCount(mesh);
    primitive.material_index = 0;
    primitive.has_indices = true;
    primitive.is_normal_mapped = false;

    ModelMesh model_mesh = {};
    model_mesh.primitives = egArrayCreate(allocator, Primitive);
    egArrayPush(&model_mesh.primitives, primitive);

    egArrayPush(&model->meshes, model_mesh);

    Node node = {};
    node.matrix = egFloat4x4Diagonal(1.0f);
    node.resolved_matrix = egFloat4x4Diagonal(1.0f);
    node.mesh_index = 0;

    egArrayPush(&model->nodes, node);

    for (size_t i = 0; i < egArrayLength(model->nodes); ++i)
    {
        Node *node = &model->nodes[i];
        if (node->parent_index == -1)
        {
            egArrayPush(&model->root_nodes, i);
        }
    }

    return model;
}

void egModelAssetDestroy(EgModelAsset *model)
{
    EgEngine *engine = model->manager->engine;
    RgDevice *device = egEngineGetDevice(engine);

    switch (model->type)
    {
    case MODEL_FROM_MESH: {
        break;
    }
    case MODEL_FROM_GLTF: {
        for (EgSampler *sampler = model->samplers;
             sampler != model->samplers + egArrayLength(model->samplers);
             ++sampler)
        {
            egEngineFreeSampler(engine, sampler);
        }

        for (EgImage *image = model->images;
             image != model->images + egArrayLength(model->images);
             ++image)
        {
            egEngineFreeImage(engine, image);
        }

        rgBufferDestroy(device, model->vertex_buffer);
        rgBufferDestroy(device, model->index_buffer);
        break;
    }
    }

    for (Node *node = model->nodes; node != model->nodes + egArrayLength(model->nodes);
         ++node)
    {
        egArrayFree(&node->children_indices);
    }

    for (ModelMesh *mesh = model->meshes;
         mesh != model->meshes + egArrayLength(model->meshes);
         ++mesh)
    {
        egArrayFree(&mesh->primitives);
    }

    egArrayFree(&model->nodes);
    egArrayFree(&model->root_nodes);
    egArrayFree(&model->meshes);
    egArrayFree(&model->materials);
    egArrayFree(&model->images);
    egArrayFree(&model->samplers);

    egFree(model->manager->allocator, model);
}

static void
NodeRender(EgModelAsset *model, Node *node, RgCmdBuffer *cmd_buffer, float4x4 *transform)
{
    (void)node;
    (void)cmd_buffer;

    EgEngine *engine = model->manager->engine;

    ModelUniform model_uniform = {};
    model_uniform.transform = egFloat4x4Mul(&node->resolved_matrix, transform);

    if (node->mesh_index != -1)
    {
        ModelMesh *mesh = &model->meshes[node->mesh_index];

        for (Primitive *primitive = mesh->primitives;
             primitive != mesh->primitives + egArrayLength(mesh->primitives);
             ++primitive)
        {
            Material *material = &model->materials[primitive->material_index];

            uint32_t model_index = egBufferPoolAllocateItem(
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
            material_uniform.brdf_image_index = egEngineGetBRDFImage(engine).index;

            uint32_t material_index = egBufferPoolAllocateItem(
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
                egBufferPoolGetBufferIndex(model->manager->camera_buffer_pool);
            pc.camera_index = model->manager->current_camera_index;

            pc.model_buffer_index =
                egBufferPoolGetBufferIndex(model->manager->model_buffer_pool);
            pc.model_index = model_index;

            pc.material_buffer_index =
                egBufferPoolGetBufferIndex(model->manager->material_buffer_pool);
            pc.material_index = material_index;

            rgCmdPushConstants(cmd_buffer, 0, sizeof(pc), &pc);

            if (primitive->has_indices)
            {
                rgCmdDrawIndexed(
                    cmd_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
            }
            else
            {
                rgCmdDraw(cmd_buffer, primitive->vertex_count, 1, 0, 0);
            }
        }
    }

    for (size_t *index = node->children_indices;
         index != node->children_indices + egArrayLength(node->children_indices);
         ++index)
    {
        Node *child = &model->nodes[*index];
        NodeRender(model, child, cmd_buffer, transform);
    }
}

void
egModelAssetRender(EgModelAsset *model, RgCmdBuffer *cmd_buffer, float4x4 *transform)
{
    EG_ASSERT(transform);
    rgCmdBindVertexBuffer(cmd_buffer, model->vertex_buffer, 0);
    rgCmdBindIndexBuffer(cmd_buffer, model->index_buffer, 0, RG_INDEX_TYPE_UINT32);
    for (Node *node = model->nodes; node != model->nodes + egArrayLength(model->nodes);
         ++node)
    {
        NodeRender(model, node, cmd_buffer, transform);
    }
}

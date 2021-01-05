#include "rg_ext.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "spirv.h"

#define RG_MAX(a, b) ((a > b) ? (a) : (b))

typedef enum {
    RG_EXT_SHADER_STAGE_VERTEX,
    RG_EXT_SHADER_STAGE_FRAGMENT,
    RG_EXT_SHADER_STAGE_COMPUTE,
} RgExtShaderStage;

enum {
    MAX_SETS = 8,
    MAX_BINDINGS = 8,
    MAX_ATTRIBUTES = 8,
};

typedef struct Id
{
    uint32_t opcode;
    uint32_t subtype_id;
    uint32_t storage_class;
    uint32_t binding;
    uint32_t set;
    uint32_t location;
    union
    {
        uint32_t vector_width;
        uint32_t type_size;
    };
    bool is_builtin;
    bool is_signed;
    bool is_buffer_block;
} Id;

typedef struct SetInfo
{
    uint32_t bindings_count;
    RgPipelineBindingType bindings[MAX_BINDINGS];
} SetInfo;

typedef struct ModuleInfo
{
    SetInfo sets[MAX_SETS];
    uint32_t sets_count;

    uint32_t vertex_stride;
    RgVertexAttribute attributes[MAX_ATTRIBUTES];
    uint32_t attributes_count;
} ModuleInfo;

static void
analyzeSpirv(
        RgExtShaderStage stage,
        bool dynamic_buffers,
        const uint32_t *code,
        size_t code_size,
        ModuleInfo *module)
{
    assert(code[0] == SpvMagicNumber);

    uint32_t id_bound = code[3];
    Id *ids = malloc(sizeof(Id) * id_bound);
    memset(ids, 0, sizeof(*ids) * id_bound);

    const uint32_t *inst = code + 5;
    while (inst != code + code_size)
    {
        uint16_t opcode = (uint16_t)inst[0];
        uint16_t word_count = (uint16_t)(inst[0] >> 16);

        switch (opcode)
        {
        case SpvOpDecorate: {
            assert(word_count >= 3);
            uint32_t id = inst[1];
            uint32_t deckind = inst[2];
            uint32_t decvalue = inst[3];

            switch (deckind)
            {
            case SpvDecorationDescriptorSet: ids[id].set = decvalue; break;
            case SpvDecorationBinding: ids[id].binding = decvalue; break;
            case SpvDecorationBuiltIn: ids[id].is_builtin = true; break;
            case SpvDecorationLocation: ids[id].location = decvalue; break;
            case SpvDecorationBufferBlock: ids[id].is_buffer_block = true; break;
            }

            break;
        }

        case SpvOpVariable: {
            assert(word_count >= 3);

            uint32_t id = inst[2];
            assert(id < id_bound);

            assert(ids[id].opcode == 0);

            ids[id].opcode = opcode;
            ids[id].subtype_id = inst[1];
            ids[id].storage_class = inst[3];
            break;
        }

        case SpvOpTypeVector:
        case SpvOpTypeFloat:
        case SpvOpTypeInt:
        case SpvOpTypeStruct:
        case SpvOpTypeImage:
        case SpvOpTypeSampler:
        case SpvOpTypeSampledImage: {
            assert(word_count >= 2);

            uint32_t id = inst[1];
            assert(id < id_bound);

            assert(ids[id].opcode == 0);
            ids[id].opcode = opcode;

            if (opcode == SpvOpTypeVector)
            {
                assert(word_count >= 4);
                ids[id].subtype_id = inst[2];
                ids[id].vector_width = inst[3];
                assert(ids[id].vector_width > 0);
            }

            if (opcode == SpvOpTypeFloat)
            {
                assert(word_count >= 3);
                ids[id].type_size = inst[2];
                assert(ids[id].type_size > 0);
            }

            if (opcode == SpvOpTypeInt)
            {
                assert(word_count >= 3);
                ids[id].type_size = inst[2];
                ids[id].is_signed = inst[3] == 1;
                assert(ids[id].type_size > 0);
            }

            break;
        }

        case SpvOpTypePointer: {
            assert(word_count == 4);

            uint32_t id = inst[1];
            assert(id < id_bound);

            assert(ids[id].opcode == 0);
            ids[id].opcode = opcode;
            ids[id].subtype_id = inst[3];
            ids[id].storage_class = inst[2];
            break;
        }
        }

        assert(inst + word_count <= code + code_size);

        inst += word_count;
    }

    memset(module, 0, sizeof(*module));

    for (Id *id = ids; id != ids + id_bound; ++id)
    {
        switch (id->opcode)
        {
        case SpvOpVariable: {
            assert(ids[id->subtype_id].opcode == SpvOpTypePointer);

            Id *pointer_type = &ids[id->subtype_id];
            Id *pointed_type = &ids[pointer_type->subtype_id];

            switch (id->storage_class)
            {
            case SpvStorageClassUniformConstant:
            case SpvStorageClassUniform:
            case SpvStorageClassStorageBuffer: {
                module->sets_count = RG_MAX(module->sets_count, id->set + 1);

                SetInfo *set = &module->sets[id->set];
                set->bindings_count = RG_MAX(set->bindings_count, id->binding + 1);

                switch (pointed_type->opcode)
                {
                case SpvOpTypeImage: set->bindings[id->binding] = RG_BINDING_IMAGE; break;
                case SpvOpTypeSampler: set->bindings[id->binding] = RG_BINDING_SAMPLER; break;
                case SpvOpTypeSampledImage:
                    set->bindings[id->binding] = RG_BINDING_IMAGE_SAMPLER;
                    break;
                case SpvOpTypeStruct:
                    if (pointed_type->is_buffer_block)
                    {
                        set->bindings[id->binding] = RG_BINDING_STORAGE_BUFFER;
                        if (dynamic_buffers)
                        {
                            set->bindings[id->binding] = RG_BINDING_STORAGE_BUFFER_DYNAMIC;
                        }
                    }
                    else if (id->storage_class == SpvStorageClassUniform)
                    {
                        set->bindings[id->binding] = RG_BINDING_UNIFORM_BUFFER;
                        if (dynamic_buffers)
                        {
                            set->bindings[id->binding] = RG_BINDING_UNIFORM_BUFFER_DYNAMIC;
                        }
                    }
                    else
                    {
                        assert(0);
                    }

                    break;
                }
                break;
            }

            case SpvStorageClassInput: {
                if (!id->is_builtin && stage == RG_EXT_SHADER_STAGE_VERTEX)
                {
                    module->attributes_count =
                        RG_MAX(module->attributes_count, id->location + 1);
                    RgVertexAttribute *attrib = &module->attributes[id->location];

                    if (pointed_type->opcode == SpvOpTypeVector)
                    {
                        uint32_t vector_width = pointed_type->vector_width;
                        Id *elem_type = &ids[pointed_type->subtype_id];

                        if (elem_type->opcode == SpvOpTypeFloat && elem_type->type_size == 32)
                        {
                            switch (vector_width)
                            {
                            case 1: attrib->format = RG_FORMAT_R32_SFLOAT; break;
                            case 2: attrib->format = RG_FORMAT_RG32_SFLOAT; break;
                            case 3: attrib->format = RG_FORMAT_RGB32_SFLOAT; break;
                            case 4: attrib->format = RG_FORMAT_RGBA32_SFLOAT; break;
                            default: assert(0); break;
                            }
                        }
                        else
                        {
                            assert(0);
                        }

                        module->vertex_stride += (vector_width * (elem_type->type_size / 8));
                    }
                    else if (pointed_type->opcode == SpvOpTypeInt)
                    {
                        if (!pointed_type->is_signed && pointed_type->type_size == 32)
                        {
                            attrib->format = RG_FORMAT_R32_UINT;
                        }
                        else
                        {
                            assert(0);
                        }

                        module->vertex_stride += (pointed_type->type_size / 8);
                    }
                    else
                    {
                        assert(0);
                    }
                }

                break;
            }

            default: break;
            }
            break;
        }
        }
    }

    size_t offset = 0;
    for (uint32_t i = 0; i < module->attributes_count; ++i)
    {
        RgVertexAttribute *attrib = &module->attributes[i];
        attrib->offset = offset;

        switch (attrib->format)
        {
        case RG_FORMAT_R32_SFLOAT: offset += 1 * 4; break;
        case RG_FORMAT_RG32_SFLOAT: offset += 2 * 4; break;
        case RG_FORMAT_RGB32_SFLOAT: offset += 3 * 4; break;
        case RG_FORMAT_RGBA32_SFLOAT: offset += 4 * 4; break;

        case RG_FORMAT_R32_UINT: offset += 4; break;
        default: assert(0); break;
        }
    }

    free(ids);
}

RgPipeline *rgExtGraphicsPipelineCreateInferredBindings(
    RgDevice *device,
    bool dynamic_buffers,
    const RgGraphicsPipelineInfo *info)
{
    ModuleInfo vertex_module;
    analyzeSpirv(
        RG_EXT_SHADER_STAGE_VERTEX,
        dynamic_buffers,
        (const uint32_t *)info->vertex,
        info->vertex_size / 4,
        &vertex_module);

    ModuleInfo fragment_module;
    analyzeSpirv(
        RG_EXT_SHADER_STAGE_FRAGMENT,
        dynamic_buffers,
        (const uint32_t *)info->fragment,
        info->fragment_size / 4,
        &fragment_module);

    uint32_t bindings_count = 0;
    RgPipelineBinding bindings[MAX_SETS * MAX_BINDINGS];
    memset(bindings, 0, sizeof(bindings));

    uint32_t modules_count = 2;
    ModuleInfo modules[] = {
        vertex_module,
        fragment_module,
    };

    ModuleInfo combined_module;
    memset(&combined_module, 0, sizeof(combined_module));

    for (uint32_t i = 0; i < modules_count; ++i)
    {
        ModuleInfo *module = &modules[i];

        for (uint32_t s = 0; s < module->sets_count; ++s)
        {
            SetInfo *set = &module->sets[s];
            combined_module.sets_count = RG_MAX(combined_module.sets_count, s + 1);
            SetInfo *combined_set = &combined_module.sets[s];

            for (uint32_t b = 0; b < set->bindings_count; ++b)
            {
                if (set->bindings[b] == 0) continue;

                combined_set->bindings_count = RG_MAX(combined_set->bindings_count, b + 1);
                if (combined_set->bindings[b] != 0)
                {
                    assert(combined_set->bindings[b] == set->bindings[b]);
                }
                combined_set->bindings[b] = set->bindings[b];
            }
        }
    }

    for (uint32_t s = 0; s < combined_module.sets_count; ++s)
    {
        SetInfo *set = &combined_module.sets[s];

        for (uint32_t b = 0; b < set->bindings_count; ++b)
        {
            RgPipelineBinding *binding = &bindings[bindings_count];
            binding->set = s;
            binding->binding = b;
            binding->type = set->bindings[b];
            bindings_count++;
        }
    }

    RgGraphicsPipelineInfo new_info = *info;

    new_info.bindings = bindings;
    new_info.num_bindings = bindings_count;

    new_info.vertex_stride = vertex_module.vertex_stride;
    new_info.num_vertex_attributes = vertex_module.attributes_count;
    new_info.vertex_attributes = vertex_module.attributes;

    return rgGraphicsPipelineCreate(device, &new_info);
}

RgPipeline *rgExtComputePipelineCreateInferredBindings(
        RgDevice *device,
        bool dynamic_buffers,
        const RgComputePipelineInfo *info)
{
    ModuleInfo module;
    memset(&module, 0, sizeof(module));

    analyzeSpirv(
        RG_EXT_SHADER_STAGE_COMPUTE,
        dynamic_buffers,
        (uint32_t *)info->code,
        info->code_size / 4,
        &module);

    uint32_t bindings_count = 0;
    RgPipelineBinding bindings[MAX_SETS * MAX_BINDINGS];
    memset(bindings, 0, sizeof(bindings));

    for (uint32_t s = 0; s < module.sets_count; ++s)
    {
        SetInfo *set = &module.sets[s];

        for (uint32_t b = 0; b < set->bindings_count; ++b)
        {
            RgPipelineBinding *binding = &bindings[bindings_count];
            binding->set = s;
            binding->binding = b;
            binding->type = set->bindings[b];
            bindings_count++;
        }
    }

    RgComputePipelineInfo new_info = *info;

    new_info.bindings = bindings;
    new_info.num_bindings = bindings_count;

    return rgComputePipelineCreate(device, &new_info);
}

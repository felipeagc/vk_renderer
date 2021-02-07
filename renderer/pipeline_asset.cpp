#include "pipeline_asset.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <tinyshader/tinyshader.h>
#include <rg.h>
#include <spirv.h>
#include "allocator.h"
#include "platform.h"
#include "engine.h"
#include "math.h"
#include "array.hpp"

enum {
    MAX_SETS = 16,
    MAX_BINDINGS = 16,
    MAX_ATTRIBUTES = 16,
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
    RgDescriptorType bindings[MAX_BINDINGS];
} SetInfo;

typedef struct ModuleInfo
{
    RgShaderStage stage;

    SetInfo sets[MAX_SETS];
    uint32_t sets_count;

    uint32_t vertex_stride;
    RgVertexAttribute attributes[MAX_ATTRIBUTES];
    uint32_t attributes_count;
} ModuleInfo;

struct PipelineAsset
{
    Allocator *allocator;
    Engine *engine;
    RgPipeline *pipeline;
};

static void AnalyzeSpirv(
    RgShaderStage stage,
    bool dynamic_buffers,
    const uint32_t *code,
    size_t code_size,
    ModuleInfo *module);

static bool isWhitespace(char c)
{
    return c == ' ' || c == '\t';
}

static bool isWhitespaceOrNewLine(char c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static bool stringToBool(const char *str, size_t len, bool *value) 
{
        if (strncmp(str, "true", len) == 0) *value = true;
        else if (strncmp(str, "false", len) == 0) *value = false;
        else return false;
        return true;
}

static bool stringToTopology(const char *str, size_t len, RgPrimitiveTopology *value)
{
    if (strncmp(str, "triangle_list", len) == 0) *value = RG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    else if (strncmp(str, "line_list", len) == 0) *value = RG_PRIMITIVE_TOPOLOGY_LINE_LIST;
    else return false;
    return true;
}

static bool stringToFrontFace(const char *str, size_t len, RgFrontFace *value)
{
    if (strncmp(str, "counter_clockwise", len) == 0) *value = RG_FRONT_FACE_COUNTER_CLOCKWISE;
    else if (strncmp(str, "clockwise", len) == 0) *value = RG_FRONT_FACE_CLOCKWISE;
    else return false;
    return true;
}

static bool stringToCullMode(const char *str, size_t len, RgCullMode * value)
{
    if (strncmp(str, "none", len) == 0) *value =  RG_CULL_MODE_NONE;
    else if (strncmp(str, "front", len) == 0) *value =  RG_CULL_MODE_FRONT;
    else if (strncmp(str, "back", len) == 0) *value =  RG_CULL_MODE_BACK;
    else if (strncmp(str, "front_and_back", len) == 0) *value = RG_CULL_MODE_FRONT_AND_BACK;
    else return false;
    return true;
}

static bool stringToPolygonMode(const char *str, size_t len, RgPolygonMode *value) 
{
    if (strncmp(str, "fill", len) == 0) *value = RG_POLYGON_MODE_FILL;
    else if (strncmp(str, "line", len) == 0) *value = RG_POLYGON_MODE_LINE;
    else if (strncmp(str, "point", len) == 0) *value = RG_POLYGON_MODE_POINT;
    else return false;
    return true;
}

static bool stringToCompareOp(const char *str, size_t len, RgCompareOp *value)
{
    if (strncmp(str, "never", len) == 0) *value = RG_COMPARE_OP_NEVER;
    if (strncmp(str, "less", len) == 0) *value = RG_COMPARE_OP_LESS;
    if (strncmp(str, "equal", len) == 0) *value = RG_COMPARE_OP_EQUAL;
    if (strncmp(str, "less_or_equal", len) == 0) *value = RG_COMPARE_OP_LESS_OR_EQUAL;
    if (strncmp(str, "greater", len) == 0) *value = RG_COMPARE_OP_GREATER;
    if (strncmp(str, "not_equal", len) == 0) *value = RG_COMPARE_OP_NOT_EQUAL;
    if (strncmp(str, "greater_or_equal", len) == 0)
        *value = RG_COMPARE_OP_GREATER_OR_EQUAL;
    if (strncmp(str, "always", len) == 0)
        *value = RG_COMPARE_OP_ALWAYS;
    else return false;
    return true;
}

PipelineAsset *PipelineAssetCreateGraphics(
        Allocator *allocator,
        Engine *engine,
        PipelineType type,
        const char *hlsl,
        size_t hlsl_size)
{
    PipelineAsset *pipeline_asset =
        (PipelineAsset*)Allocate(allocator, sizeof(PipelineAsset));
    *pipeline_asset = {};

    pipeline_asset->allocator = allocator;
    pipeline_asset->engine = engine;

    Platform *platform = EngineGetPlatform(pipeline_asset->engine);
    RgDevice *device = PlatformGetDevice(platform);

    uint8_t *vertex_code = NULL;
    size_t vertex_code_size = 0;

    // Compile vertex
    {
        TsCompilerOptions *options = tsCompilerOptionsCreate();
        tsCompilerOptionsSetStage(options, TS_SHADER_STAGE_VERTEX);
        tsCompilerOptionsSetEntryPoint(options, "vertex", strlen("vertex"));
        tsCompilerOptionsSetSource(options, hlsl, hlsl_size, NULL, 0);

        TsCompilerOutput *output = tsCompile(options);
        const char *errors = tsCompilerOutputGetErrors(output);
        if (errors)
        {
            fprintf(stderr, "Shader compilation error:\n%s\n", errors);

            tsCompilerOutputDestroy(output);
            tsCompilerOptionsDestroy(options);
            exit(1);
        }

        size_t spirv_size = 0;
        const uint8_t *spirv = tsCompilerOutputGetSpirv(output, &spirv_size);

        vertex_code = (uint8_t*)Allocate(allocator, spirv_size);
        memcpy(vertex_code, spirv, spirv_size);
        vertex_code_size = spirv_size;

        tsCompilerOutputDestroy(output);
        tsCompilerOptionsDestroy(options);
    }

    uint8_t *fragment_code = NULL;
    size_t fragment_code_size = 0;

    // Compile fragment
    {
        TsCompilerOptions *options = tsCompilerOptionsCreate();
        tsCompilerOptionsSetStage(options, TS_SHADER_STAGE_FRAGMENT);
        tsCompilerOptionsSetEntryPoint(options, "pixel", strlen("pixel"));
        tsCompilerOptionsSetSource(options, hlsl, hlsl_size, NULL, 0);

        TsCompilerOutput *output = tsCompile(options);
        const char *errors = tsCompilerOutputGetErrors(output);
        if (errors)
        {
            fprintf(stderr, "Shader compilation error:\n%s\n", errors);

            tsCompilerOutputDestroy(output);
            tsCompilerOptionsDestroy(options);
            exit(1);
        }

        size_t spirv_size = 0;
        const uint8_t *spirv = tsCompilerOutputGetSpirv(output, &spirv_size);

        fragment_code = (uint8_t*)Allocate(allocator, spirv_size);
        memcpy(fragment_code, spirv, spirv_size);
        fragment_code_size = spirv_size;

        tsCompilerOutputDestroy(output);
        tsCompilerOptionsDestroy(options);
    }

    RgGraphicsPipelineInfo pipeline_info = {};
    pipeline_info.polygon_mode = RG_POLYGON_MODE_FILL;
    pipeline_info.cull_mode = RG_CULL_MODE_NONE;
    pipeline_info.front_face = RG_FRONT_FACE_CLOCKWISE;
    pipeline_info.topology = RG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_info.blend.enable = false;
    pipeline_info.depth_stencil.test_enable = true;
    pipeline_info.depth_stencil.write_enable = true;
    pipeline_info.depth_stencil.bias_enable = false;
    pipeline_info.depth_stencil.compare_op = RG_COMPARE_OP_GREATER;

    pipeline_info.vertex = vertex_code;
    pipeline_info.vertex_size = vertex_code_size;
    pipeline_info.vertex_entry = "vertex";

    pipeline_info.fragment = fragment_code;
    pipeline_info.fragment_size = fragment_code_size;
    pipeline_info.fragment_entry = "pixel";

    const char *pragma = "#pragma";
    size_t pragma_len = strlen(pragma);

    for (size_t i = 0; i < hlsl_size; ++i)
    {
        size_t len = hlsl_size - i;
        if (hlsl[i] == '#' && len > pragma_len && strncmp(&hlsl[i], pragma, pragma_len) == 0)
        {
            i += pragma_len;
            while (isWhitespace(hlsl[i])) i++;

            size_t key_start = i;
            while (!isWhitespaceOrNewLine(hlsl[i])) i++;
            size_t key_end = i;

            while (isWhitespace(hlsl[i])) i++;

            size_t value_start = i;
            while (!isWhitespaceOrNewLine(hlsl[i])) i++;
            size_t value_end = i;

            const char *key = &hlsl[key_start];
            size_t key_len = key_end - key_start;

            const char *value = &hlsl[value_start];
            size_t value_len = value_end - value_start;

            bool success = true;
            if (strncmp(key, "blend", key_len) == 0)
            {
                 success = stringToBool(value, value_len, &pipeline_info.blend.enable);
            }
            else if (strncmp(key, "depth_test", key_len) == 0)
            {
                 success = stringToBool(
                         value, value_len, &pipeline_info.depth_stencil.test_enable);
            }
            else if (strncmp(key, "depth_write", key_len) == 0)
            {
                 success = stringToBool(
                         value, value_len, &pipeline_info.depth_stencil.write_enable);
            }
            else if (strncmp(key, "depth_bias", key_len) == 0)
            {
                success = stringToBool(
                        value, value_len, &pipeline_info.depth_stencil.bias_enable);
            }
            else if (strncmp(key, "depth_compare_op", key_len) == 0)
            {
                success = stringToCompareOp(
                        value, value_len, &pipeline_info.depth_stencil.compare_op);
            }
            else if (strncmp(key, "topology", key_len) == 0)
            {
                 success = stringToTopology(value, value_len, &pipeline_info.topology);
            }
            else if (strncmp(key, "polygon_mode", key_len) == 0)
            {
                success = stringToPolygonMode(value, value_len, &pipeline_info.polygon_mode);
            }
            else if (strncmp(key, "cull_mode", key_len) == 0)
            {
                success = stringToCullMode(value, value_len, &pipeline_info.cull_mode);
            }
            else if (strncmp(key, "front_face", key_len) == 0)
            {
                success = stringToFrontFace(value, value_len, &pipeline_info.front_face);
            }
            else
            {
                success = false;
            }

            if (!success)
            {
                fprintf(stderr, "Warning: invalid pipeline parameter: '%.*s': '%.*s'\n",
                        (int)key_len, key, (int)value_len, value);
            }
        }
    }

    ModuleInfo vertex_module = {};
    AnalyzeSpirv(
        RG_SHADER_STAGE_VERTEX,
        true,
        (uint32_t *)vertex_code,
        vertex_code_size / 4,
        &vertex_module);

    pipeline_info.vertex_stride = vertex_module.vertex_stride;
    pipeline_info.num_vertex_attributes = vertex_module.attributes_count;
    pipeline_info.vertex_attributes = vertex_module.attributes;

    pipeline_info.pipeline_layout = EngineGetPipelineLayout(engine, type);

    pipeline_asset->pipeline = rgGraphicsPipelineCreate(
                device,
                &pipeline_info);

    Free(allocator, vertex_code);
    Free(allocator, fragment_code);

    return pipeline_asset;
}

void PipelineAssetDestroy(PipelineAsset *pipeline_asset)
{
    Platform *platform = EngineGetPlatform(pipeline_asset->engine);
    RgDevice *device = PlatformGetDevice(platform);

    rgPipelineDestroy(device, pipeline_asset->pipeline);

    Free(pipeline_asset->allocator, pipeline_asset);
}

RgPipeline *PipelineAssetGetPipeline(PipelineAsset *pipeline_asset)
{
    return pipeline_asset->pipeline;
}

static void AnalyzeSpirv(
    RgShaderStage stage,
    bool dynamic_buffers,
    const uint32_t *code,
    size_t code_size,
    ModuleInfo *module)
{
    memset(module, 0, sizeof(*module));

    assert(code[0] == SpvMagicNumber);

    module->stage = stage;

    uint32_t id_bound = code[3];
    Id *ids = (Id*)malloc(sizeof(Id) * id_bound);
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
                module->sets_count = max(module->sets_count, id->set + 1);

                SetInfo *set = &module->sets[id->set];
                set->bindings_count = max(set->bindings_count, id->binding + 1);

                switch (pointed_type->opcode)
                {
                case SpvOpTypeImage: set->bindings[id->binding] = RG_DESCRIPTOR_IMAGE; break;
                case SpvOpTypeSampler: set->bindings[id->binding] = RG_DESCRIPTOR_SAMPLER; break;
                case SpvOpTypeSampledImage:
                    set->bindings[id->binding] = RG_DESCRIPTOR_IMAGE_SAMPLER;
                    break;
                case SpvOpTypeStruct:
                    if (pointed_type->is_buffer_block)
                    {
                        set->bindings[id->binding] = RG_DESCRIPTOR_STORAGE_BUFFER;
                        if (dynamic_buffers)
                        {
                            set->bindings[id->binding] = RG_DESCRIPTOR_STORAGE_BUFFER_DYNAMIC;
                        }
                    }
                    else if (id->storage_class == SpvStorageClassUniform)
                    {
                        set->bindings[id->binding] = RG_DESCRIPTOR_UNIFORM_BUFFER;
                        if (dynamic_buffers)
                        {
                            set->bindings[id->binding] = RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC;
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
                if (!id->is_builtin && stage == RG_SHADER_STAGE_VERTEX)
                {
                    module->attributes_count =
                        max(module->attributes_count, id->location + 1);
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
        attrib->offset = (uint32_t)offset;

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

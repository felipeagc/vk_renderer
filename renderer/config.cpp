#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "lexer.h"
#include "string_builder.h"
#include "array.h"
#include "string_map.hpp"

struct EgConfig
{
    EgAllocator *allocator;
    EgArena *arena;
    EgConfigValue *root;
};

struct EgConfigValue
{
    EgConfigValueType type;
    union
    {
        const char *str;
        int64_t int_;
        double float_;
        EgArray(EgConfigValue*) array;
        EgStringMap<EgConfigValue*> object;
    };
};

static EgConfigValue *NewValue(EgConfig *config, EgConfigValueType type)
{
    EgAllocator *arena = egArenaGetAllocator(config->arena);
    EgConfigValue *value = (EgConfigValue*)egAllocate(arena, sizeof(*value));
    *value = {};
    value->type = type;

    switch (value->type)
    {
    case CONFIG_VALUE_STRING:
        value->str = nullptr;
        break;
    case CONFIG_VALUE_OBJECT:
        value->object = EgStringMap<EgConfigValue*>::create(arena);
        break;
    case CONFIG_VALUE_ARRAY:
        value->array = egArrayCreate(arena, EgConfigValue*);
        break;
    default: break;
    }

    return value;
}

EgConfig *egConfigCreate(EgAllocator *allocator)
{
    EgConfig *config = (EgConfig*)egAllocate(allocator, sizeof(*config));
    *config = {};
    config->allocator = allocator;
    config->root = NewValue(config, CONFIG_VALUE_OBJECT);

    return config;
}

static EgToken PeekToken(EgAllocator *allocator, EgTokenizerState state)
{
    EgToken new_token = {};
    egTokenizerNextToken(allocator, state, &new_token);
    return new_token;
}

static bool ExpectToken(EgAllocator *allocator, EgTokenizerState *state, EgTokenType type, EgToken *token)
{
    EgToken new_token = {};
    EgTokenizerState new_state = egTokenizerNextToken(allocator, *state, &new_token);
    if (new_token.type == TOKEN_ERROR)
    {
        fprintf(stderr, "EgConfig parse error: %s\n", new_token.str);
        return false;
    }

    if (new_token.type == type)
    {
        *state = new_state;
        if (token) *token = new_token;
        return true;
    }

    fprintf(
        stderr,
        "EgConfig parse error:%lu: unexpected token: %u, expected: %u\n",
        new_token.pos,
        new_token.type,
        type);
    return false;
}

static EgConfigValue *ParseValue(EgConfig *config, EgTokenizerState *state)
{
    EgAllocator *arena = egArenaGetAllocator(config->arena);

    EgToken first_token = PeekToken(arena, *state);
    switch (first_token.type)
    {
    case TOKEN_LCURLY:
    {
        EgConfigValue *value = NewValue(config, CONFIG_VALUE_OBJECT);
        if (!ExpectToken(arena, state, TOKEN_LCURLY, nullptr)) return nullptr;

        while (PeekToken(arena, *state).type == TOKEN_IDENT)
        {
            EgToken ident_token = {};
            if (!ExpectToken(arena, state, TOKEN_IDENT, &ident_token)) return nullptr;

            if (!ExpectToken(arena, state, TOKEN_COLON, nullptr)) return nullptr;

            EgConfigValue *field_value = ParseValue(config, state);
            if (!field_value) return nullptr;
            value->object.set(ident_token.str, field_value);

            if (PeekToken(arena, *state).type != TOKEN_RCURLY)
            {
                if (!ExpectToken(arena, state, TOKEN_COMMA, nullptr)) return nullptr;
            }
        }

        if (!ExpectToken(arena, state, TOKEN_RCURLY, nullptr)) return nullptr;

        return value;
    }

    case TOKEN_LBRACKET:
    {
        EgConfigValue *value = NewValue(config, CONFIG_VALUE_ARRAY);
        if (!ExpectToken(arena, state, TOKEN_LBRACKET, nullptr)) return nullptr;

        while (PeekToken(arena, *state).type != TOKEN_RBRACKET)
        {
            EgConfigValue *elem_value = ParseValue(config, state);
            if (!elem_value) return nullptr;

            egArrayPush(&value->array, elem_value);

            if (PeekToken(arena, *state).type != TOKEN_RBRACKET)
            {
                if (!ExpectToken(arena, state, TOKEN_COMMA, nullptr)) return nullptr;
            }
        }

        if (!ExpectToken(arena, state, TOKEN_RBRACKET, nullptr)) return nullptr;

        return value;
    }

    case TOKEN_STRING:
    {
        EgToken str_token = {};
        if (!ExpectToken(arena, state, TOKEN_STRING, &str_token)) return nullptr;
        EgConfigValue *value = NewValue(config, CONFIG_VALUE_STRING);
        value->str = str_token.str;
        return value;
    }

    case TOKEN_ERROR:
    {
        fprintf(
            stderr,
            "EgConfig parse error:%lu: %s\n",
            first_token.pos,
            first_token.str);
        break;
    }
    
    default:
    {
        fprintf(
            stderr,
            "EgConfig parse error:%lu: unexpected token: %u\n",
            first_token.pos,
            first_token.type);
        break;
    }
    }
    
    return nullptr;
}

EgConfig *egConfigParse(EgAllocator *allocator, const char *text, size_t text_length)
{
    (void)text;
    (void)text_length;
    EgConfig *config = (EgConfig*)egAllocate(allocator, sizeof(*config));
    *config = {};
    config->allocator = allocator;
    config->arena = egArenaCreate(allocator, 1 << 12);

    EgTokenizerState state = egTokenizerCreate(text, text_length);
    config->root = ParseValue(config, &state);

    if (!config->root)
    {
        egConfigFree(config);
        return nullptr;
    }

    return config;
}

void egConfigFree(EgConfig *config)
{
    if (!config) return;

    egArenaDestroy(config->arena);
    egFree(config->allocator, config);
}

EgConfigValue *egConfigGetRoot(EgConfig *config)
{
    return config->root;
}

EgConfigValueType egConfigValueGetType(EgConfigValue *value)
{
    return value->type;
}

int64_t egConfigValueGetInt(EgConfigValue *value, int64_t default_value)
{
    if (value->type != CONFIG_VALUE_INT) return default_value;
    return value->int_; 
}

double egConfigValueGetFloat(EgConfigValue *value, double default_value)
{
    if (value->type != CONFIG_VALUE_FLOAT) return default_value;
    return value->float_; 
}

const char *egConfigValueGetString(EgConfigValue *value)
{
    if (value->type != CONFIG_VALUE_STRING) return nullptr;
    return value->str;
}

EgConfigValue *egConfigValueObjectGetField(EgConfigValue *value, const char *name)
{
    if (value->type != CONFIG_VALUE_OBJECT) return nullptr;
    EgConfigValue *field_value = nullptr;
    if (!value->object.get(name, &field_value)) return nullptr;
    
    EG_ASSERT(field_value);
    return field_value;
}

size_t egConfigValueObjectGetAllFields(
    EgConfigValue *value,
    EgAllocator *allocator,
    const char ***names,
    EgConfigValue ***values)
{
    size_t length = value->object.length();

    *names = (const char**)egAllocate(allocator, sizeof(char*) * length);
    *values = (EgConfigValue**)egAllocate(allocator, sizeof(EgConfigValue*) * length);

    size_t index = 0;
    for (auto &slot : value->object)
    {
        (*names)[index] = slot.key;
        (*values)[index] = slot.value;
        index++;
    }

    return length;
}

size_t egConfigValueArrayGetLength(EgConfigValue *value)
{
    if (value->type != CONFIG_VALUE_ARRAY) return 0;
    return egArrayLength(value->array);
}

EgConfigValue *egConfigValueArrayGetElement(EgConfigValue *value, size_t index)
{
    if (value->type != CONFIG_VALUE_ARRAY) return nullptr;
    return value->array[index];
}

static void PrintIndent(EgStringBuilder *sb, size_t indent)
{
    for (size_t i = 0; i < indent; ++i)
    {
        egStringBuilderAppend(sb, "  ");
    }
}

static void EgConfigValueSprint(EgConfigValue *value, EgStringBuilder *sb, size_t indent)
{
    (void)sb;

    switch (value->type)
    {
    case CONFIG_VALUE_INT:
    {
        egStringBuilderAppendFormat(sb, "%ld", value->int_);
        break;
    }
    case CONFIG_VALUE_FLOAT:
    {
        egStringBuilderAppendFormat(sb, "%lf", value->float_);
        break;
    }
    case CONFIG_VALUE_STRING:
    {
        egStringBuilderAppendFormat(sb, "\"%s\"", value->str);
        break;
    }
    case CONFIG_VALUE_ARRAY:
    {
        egStringBuilderAppend(sb, "[\n");

        for (size_t i = 0; i < egArrayLength(value->array); ++i)
        {
            PrintIndent(sb, indent+1);
            EgConfigValueSprint(value->array[i], sb, indent+1);
            egStringBuilderAppend(sb, ",\n");
        }

        PrintIndent(sb, indent);
        egStringBuilderAppend(sb, "]");
        break;
    }
    case CONFIG_VALUE_OBJECT:
    {
        egStringBuilderAppend(sb, "{\n");

        for (auto &slot : value->object)
        {
            PrintIndent(sb, indent+1);
            egStringBuilderAppend(sb, slot.key);
            egStringBuilderAppend(sb, ": ");
            EgConfigValueSprint(slot.value, sb, indent+1);
            egStringBuilderAppend(sb, ",\n");
        }

        PrintIndent(sb, indent);
        egStringBuilderAppend(sb, "}");
        break;
    }
    }
}

const char *egConfigSprint(EgConfig *config, EgAllocator *allocator)
{
    if (!config) return nullptr;

    EgStringBuilder *sb = egStringBuilderCreate(allocator);

    EgConfigValueSprint(config->root, sb, 0);

    const char *str = egStringBuilderBuild(sb, allocator);
    egStringBuilderDestroy(sb);
    return str;
}

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "lexer.h"
#include "string_builder.h"
#include "array.hpp"
#include "string_map.hpp"

struct Config
{
    Allocator *allocator;
    Arena *arena;
    ConfigValue *root;
};

struct ConfigValue
{
    ConfigValueType type;
    union
    {
        const char *str;
        int64_t int_;
        double float_;
        Array<ConfigValue*> array;
        StringMap<ConfigValue*> object;
    };
};

static ConfigValue *NewValue(Config *config, ConfigValueType type)
{
    Allocator *arena = ArenaGetAllocator(config->arena);
    ConfigValue *value = (ConfigValue*)Allocate(arena, sizeof(*value));
    *value = {};
    value->type = type;

    switch (value->type)
    {
    case CONFIG_VALUE_STRING:
        value->str = nullptr;
        break;
    case CONFIG_VALUE_OBJECT:
        value->object = StringMap<ConfigValue*>::create(arena);
        break;
    case CONFIG_VALUE_ARRAY:
        value->array = Array<ConfigValue*>::create(arena);
        break;
    default: break;
    }

    return value;
}

Config *ConfigCreate(Allocator *allocator)
{
    Config *config = (Config*)Allocate(allocator, sizeof(*config));
    *config = {};
    config->allocator = allocator;
    config->root = NewValue(config, CONFIG_VALUE_OBJECT);

    return config;
}

static Token PeekToken(Allocator *allocator, TokenizerState state)
{
    Token new_token = {};
    NextToken(allocator, state, &new_token);
    return new_token;
}

static bool ExpectToken(Allocator *allocator, TokenizerState *state, TokenType type, Token *token)
{
    Token new_token = {};
    TokenizerState new_state = NextToken(allocator, *state, &new_token);
    if (new_token.type == TOKEN_ERROR)
    {
        fprintf(stderr, "Config parse error: %s\n", new_token.str);
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
        "Config parse error:%lu: unexpected token: %u, expected: %u\n",
        new_token.pos,
        new_token.type,
        type);
    return false;
}

static ConfigValue *ParseValue(Config *config, TokenizerState *state)
{
    Allocator *arena = ArenaGetAllocator(config->arena);

    Token first_token = PeekToken(arena, *state);
    switch (first_token.type)
    {
    case TOKEN_LCURLY:
    {
        ConfigValue *value = NewValue(config, CONFIG_VALUE_OBJECT);
        if (!ExpectToken(arena, state, TOKEN_LCURLY, nullptr)) return nullptr;

        while (PeekToken(arena, *state).type == TOKEN_IDENT)
        {
            Token ident_token = {};
            if (!ExpectToken(arena, state, TOKEN_IDENT, &ident_token)) return nullptr;

            if (!ExpectToken(arena, state, TOKEN_COLON, nullptr)) return nullptr;

            ConfigValue *field_value = ParseValue(config, state);
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
        ConfigValue *value = NewValue(config, CONFIG_VALUE_ARRAY);
        if (!ExpectToken(arena, state, TOKEN_LBRACKET, nullptr)) return nullptr;

        while (PeekToken(arena, *state).type != TOKEN_RBRACKET)
        {
            ConfigValue *elem_value = ParseValue(config, state);
            if (!elem_value) return nullptr;

            value->array.push_back(elem_value);

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
        Token str_token = {};
        if (!ExpectToken(arena, state, TOKEN_STRING, &str_token)) return nullptr;
        ConfigValue *value = NewValue(config, CONFIG_VALUE_STRING);
        value->str = str_token.str;
        return value;
    }

    case TOKEN_ERROR:
    {
        fprintf(
            stderr,
            "Config parse error:%lu: %s\n",
            first_token.pos,
            first_token.str);
        break;
    }
    
    default:
    {
        fprintf(
            stderr,
            "Config parse error:%lu: unexpected token: %u\n",
            first_token.pos,
            first_token.type);
        break;
    }
    }
    
    return nullptr;
}

Config *ConfigParse(Allocator *allocator, const char *text, size_t text_length)
{
    (void)text;
    (void)text_length;
    Config *config = (Config*)Allocate(allocator, sizeof(*config));
    *config = {};
    config->allocator = allocator;
    config->arena = ArenaCreate(allocator, 1 << 12);

    TokenizerState state = NewTokenizerState(text, text_length);
    config->root = ParseValue(config, &state);

    if (!config->root)
    {
        ConfigFree(config);
        return nullptr;
    }

    return config;
}

void ConfigFree(Config *config)
{
    if (!config) return;

    ArenaDestroy(config->arena);
    Free(config->allocator, config);
}

ConfigValue *ConfigGetRoot(Config *config)
{
    return config->root;
}

ConfigValueType ConfigValueGetType(ConfigValue *value)
{
    return value->type;
}

int64_t ConfigValueGetInt(ConfigValue *value, int64_t default_value)
{
    if (value->type != CONFIG_VALUE_INT) return default_value;
    return value->int_; 
}

double ConfigValueGetFloat(ConfigValue *value, double default_value)
{
    if (value->type != CONFIG_VALUE_FLOAT) return default_value;
    return value->float_; 
}

const char *ConfigValueGetString(ConfigValue *value)
{
    if (value->type != CONFIG_VALUE_STRING) return nullptr;
    return value->str;
}

ConfigValue *ConfigValueObjectGetField(ConfigValue *value, const char *name)
{
    if (value->type != CONFIG_VALUE_OBJECT) return nullptr;
    ConfigValue *field_value = nullptr;
    if (!value->object.get(name, &field_value)) return nullptr;
    
    assert(field_value);
    return field_value;
}

size_t ConfigValueObjectGetAllFields(
    ConfigValue *value,
    Allocator *allocator,
    const char ***names,
    ConfigValue ***values)
{
    size_t length = value->object.length();

    *names = (const char**)Allocate(allocator, sizeof(char*) * length);
    *values = (ConfigValue**)Allocate(allocator, sizeof(ConfigValue*) * length);

    size_t index = 0;
    for (auto &slot : value->object)
    {
        (*names)[index] = slot.key;
        (*values)[index] = slot.value;
        index++;
    }

    return length;
}

size_t ConfigValueArrayGetLength(ConfigValue *value)
{
    if (value->type != CONFIG_VALUE_ARRAY) return 0;
    return value->array.length;
}

ConfigValue *ConfigValueArrayGetElement(ConfigValue *value, size_t index)
{
    if (value->type != CONFIG_VALUE_ARRAY) return nullptr;
    return value->array[index];
}

static void PrintIndent(StringBuilder *sb, size_t indent)
{
    for (size_t i = 0; i < indent; ++i)
    {
        StringBuilderAppend(sb, "  ");
    }
}

static void ConfigValueSprint(ConfigValue *value, StringBuilder *sb, size_t indent)
{
    (void)sb;

    switch (value->type)
    {
    case CONFIG_VALUE_INT:
    {
        StringBuilderAppendFormat(sb, "%ld", value->int_);
        break;
    }
    case CONFIG_VALUE_FLOAT:
    {
        StringBuilderAppendFormat(sb, "%lf", value->float_);
        break;
    }
    case CONFIG_VALUE_STRING:
    {
        StringBuilderAppendFormat(sb, "\"%s\"", value->str);
        break;
    }
    case CONFIG_VALUE_ARRAY:
    {
        StringBuilderAppend(sb, "[\n");

        for (size_t i = 0; i < value->array.length; ++i)
        {
            PrintIndent(sb, indent+1);
            ConfigValueSprint(value->array[i], sb, indent+1);
            StringBuilderAppend(sb, ",\n");
        }

        PrintIndent(sb, indent);
        StringBuilderAppend(sb, "]");
        break;
    }
    case CONFIG_VALUE_OBJECT:
    {
        StringBuilderAppend(sb, "{\n");

        for (auto &slot : value->object)
        {
            PrintIndent(sb, indent+1);
            StringBuilderAppend(sb, slot.key);
            StringBuilderAppend(sb, ": ");
            ConfigValueSprint(slot.value, sb, indent+1);
            StringBuilderAppend(sb, ",\n");
        }

        PrintIndent(sb, indent);
        StringBuilderAppend(sb, "}");
        break;
    }
    }
}

const char *ConfigSprint(Config *config, Allocator *allocator)
{
    if (!config) return nullptr;

    StringBuilder *sb = StringBuilderCreate(allocator);

    ConfigValueSprint(config->root, sb, 0);

    const char *str = StringBuilderBuild(sb, allocator);
    StringBuilderDestroy(sb);
    return str;
}

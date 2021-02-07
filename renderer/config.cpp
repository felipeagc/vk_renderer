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
    ConfigValue *value = (ConfigValue*)Allocate(config->allocator, sizeof(*value));
    *value = {};
    value->type = type;

    switch (value->type)
    {
    case CONFIG_VALUE_STRING:
        value->str = nullptr;
        break;
    case CONFIG_VALUE_OBJECT:
        value->object = StringMap<ConfigValue*>::create(config->allocator);
        break;
    case CONFIG_VALUE_ARRAY:
        value->array = Array<ConfigValue*>::create(config->allocator);
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

static Token PeekToken(Config *config, TokenizerState state)
{
    Token new_token = {};
    NextToken(config->allocator, state, &new_token);
    if (new_token.type == TOKEN_ERROR)
    {
    }

    return new_token;
}

static bool ExpectToken(Config *config, TokenizerState *state, TokenType type, Token *token)
{
    Token new_token = {};
    TokenizerState new_state = NextToken(config->allocator, *state, &new_token);
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
    Token first_token = PeekToken(config, *state);
    switch (first_token.type)
    {
    case TOKEN_LCURLY:
    {
        ConfigValue *value = NewValue(config, CONFIG_VALUE_OBJECT);
        if (!ExpectToken(config, state, TOKEN_LCURLY, nullptr)) return nullptr;

        while (PeekToken(config, *state).type == TOKEN_IDENT)
        {
            Token ident_token = {};
            if (!ExpectToken(config, state, TOKEN_IDENT, &ident_token)) return nullptr;

            if (!ExpectToken(config, state, TOKEN_COLON, nullptr)) return nullptr;

            ConfigValue *field_value = ParseValue(config, state);
            if (!field_value) return nullptr;
            value->object.set(ident_token.str, field_value);

            if (PeekToken(config, *state).type != TOKEN_RCURLY)
            {
                if (!ExpectToken(config, state, TOKEN_COMMA, nullptr)) return nullptr;
            }
        }

        if (!ExpectToken(config, state, TOKEN_RCURLY, nullptr)) return nullptr;

        return value;
    }

    case TOKEN_LBRACKET:
    {
        ConfigValue *value = NewValue(config, CONFIG_VALUE_ARRAY);
        if (!ExpectToken(config, state, TOKEN_LBRACKET, nullptr)) return nullptr;

        while (PeekToken(config, *state).type != TOKEN_RBRACKET)
        {
            ConfigValue *elem_value = ParseValue(config, state);
            if (!elem_value) return nullptr;

            value->array.push_back(elem_value);

            if (PeekToken(config, *state).type != TOKEN_RBRACKET)
            {
                if (!ExpectToken(config, state, TOKEN_COMMA, nullptr)) return nullptr;
            }
        }

        if (!ExpectToken(config, state, TOKEN_RBRACKET, nullptr)) return nullptr;

        return value;
    }

    case TOKEN_STRING:
    {
        Token str_token = {};
        if (!ExpectToken(config, state, TOKEN_STRING, &str_token)) return nullptr;
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

    TokenizerState state = NewTokenizerState(text, text_length);
    config->root = ParseValue(config, &state);
    if (!config->root)
    {
        ConfigFree(config);
        return nullptr;
    }

    return config;
}

static void ConfigValueFree(Allocator *allocator, ConfigValue *value)
{
    switch (value->type)
    {
    case CONFIG_VALUE_INT:
    case CONFIG_VALUE_FLOAT: break;
    case CONFIG_VALUE_STRING:
    {
        if (value->str)
        {
            Free(allocator, (void*)value->str);
        }
        break;
    }
    case CONFIG_VALUE_OBJECT:
    {
        for (auto &slot : value->object)
        {
            ConfigValue *field_value = slot.value;
            ConfigValueFree(allocator, field_value);
        }
        value->object.free();
        break;
    }
    case CONFIG_VALUE_ARRAY:
    {
        for (ConfigValue *elem_value : value->array)
        {
            ConfigValueFree(allocator, elem_value);
        }
        value->array.free();
        break;
    }
    }
    Free(allocator, value);
}

void ConfigFree(Config *config)
{
    if (!config) return;

    (void)config;
    if (config->root)
    {
        ConfigValueFree(config->allocator, config->root);
    }
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

const char *ConfigValueGetString(ConfigValue *value, const char *default_value)
{
    if (value->type != CONFIG_VALUE_STRING) return default_value;
    return value->str;
}

ConfigValue *ConfigValueGetField(ConfigValue *value, const char *name)
{
    if (value->type != CONFIG_VALUE_OBJECT) return nullptr;
    ConfigValue *field_value = nullptr;
    if (!value->object.get(name, &field_value)) return nullptr;
    
    assert(field_value);
    return field_value;
}

size_t ConfigValueGetArrayLength(ConfigValue *value)
{
    if (value->type != CONFIG_VALUE_ARRAY) return 0;
    return value->array.length;
}

ConfigValue *ConfigValueGetElement(ConfigValue *value, size_t index)
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

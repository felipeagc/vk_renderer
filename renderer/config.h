#pragma once

#include "base.h"

typedef struct Allocator Allocator;

typedef enum ConfigValueType
{
    CONFIG_VALUE_STRING,
    CONFIG_VALUE_INT,
    CONFIG_VALUE_FLOAT,
    CONFIG_VALUE_OBJECT,
    CONFIG_VALUE_ARRAY,
} ConfigValueType;

typedef struct Config Config;
typedef struct ConfigValue ConfigValue;

Config *ConfigCreate(Allocator *allocator);
Config *ConfigParse(Allocator *allocator, const char *text, size_t text_length);
void ConfigFree(Config *config);
const char *ConfigSprint(Config *config, Allocator *allocator);
ConfigValue *ConfigGetRoot(Config *config);

ConfigValueType ConfigValueGetType(ConfigValue *value);

int64_t ConfigValueGetInt(ConfigValue *value, int64_t default_value);
double ConfigValueGetFloat(ConfigValue *value, double default_value);
const char *ConfigValueGetString(ConfigValue *value);

ConfigValue *ConfigValueObjectGetField(ConfigValue *value, const char *name);
size_t ConfigValueObjectGetAllFields(
    ConfigValue *value,
    Allocator *allocator,
    const char ***names,
    ConfigValue ***values);

size_t ConfigValueArrayGetLength(ConfigValue *value);
ConfigValue *ConfigValueArrayGetElement(ConfigValue *value, size_t index);

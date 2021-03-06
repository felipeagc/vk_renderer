#pragma once

#include "base.h"

typedef struct EgAllocator EgAllocator;

typedef enum EgConfigValueType {
    CONFIG_VALUE_STRING,
    CONFIG_VALUE_INT,
    CONFIG_VALUE_FLOAT,
    CONFIG_VALUE_OBJECT,
    CONFIG_VALUE_ARRAY,
} EgConfigValueType;

typedef struct EgConfig EgConfig;
typedef struct EgConfigValue EgConfigValue;

EgConfig *egConfigCreate(EgAllocator *allocator);
EgConfig *egConfigParse(EgAllocator *allocator, const char *text, size_t text_length);
void egConfigFree(EgConfig *config);
const char *egConfigSprint(EgConfig *config, EgAllocator *allocator);
EgConfigValue *egConfigGetRoot(EgConfig *config);

EgConfigValueType egConfigValueGetType(EgConfigValue *value);

int64_t egConfigValueGetInt(EgConfigValue *value, int64_t default_value);
double egConfigValueGetFloat(EgConfigValue *value, double default_value);
const char *egConfigValueGetString(EgConfigValue *value);

EgConfigValue *egConfigValueObjectGetField(EgConfigValue *value, const char *name);
size_t egConfigValueObjectGetAllFields(
    EgConfigValue *value, EgAllocator *allocator, const char ***names, EgConfigValue ***values);

size_t egConfigValueArrayGetLength(EgConfigValue *value);
EgConfigValue *egConfigValueArrayGetElement(EgConfigValue *value, size_t index);

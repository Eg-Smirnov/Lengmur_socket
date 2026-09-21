// Generated from ../config.json by tools/generate_config.py. Do not edit.
#pragma once

#include <stddef.h>

enum class FieldType { Int32, Bool, String };
struct FieldDefinition { const char *name; FieldType type; };

constexpr FieldDefinition kFields[] = {
    {"setting_1", FieldType::Int32},
    {"setting_2", FieldType::Int32},
    {"setting_3", FieldType::Int32},
    {"setting_str", FieldType::String},
};
constexpr size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);
constexpr char kDefaultConfig[] = "{\"setting_1\":0,\"setting_2\":0,\"setting_3\":0,\"setting_str\":\"aboab\"}";

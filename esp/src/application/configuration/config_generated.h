// Generated from config.json by tools/generate_config.py. Do not edit.
#pragma once

#include <stddef.h>

enum class FieldType { Int32, Bool, String, ShuntResistance };
struct FieldDefinition { const char *name; FieldType type; };

constexpr FieldDefinition kFields[] = {
    {"sample_rate", FieldType::Int32},
    {"min_voltage", FieldType::Int32},
    {"max_voltage", FieldType::Int32},
    {"shunt_res", FieldType::ShuntResistance},
};
constexpr size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);
constexpr char kDefaultConfig[] = "{\"sample_rate\":1000,\"min_voltage\":-10,\"max_voltage\":10,\"shunt_res\":0}";

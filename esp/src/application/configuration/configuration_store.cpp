#include "configuration_store.h"

#include <Preferences.h>
#include <string.h>

#include "config_generated.h"

namespace {

constexpr char kNamespace[] = "lengmur";
constexpr char kConfigKey[] = "config";

const FieldDefinition *findField(const char *name) {
  if (name != nullptr) {
    for (const FieldDefinition &field : kFields) {
      if (strcmp(name, field.name) == 0) {
        return &field;
      }
    }
  }
  return nullptr;
}

}  // namespace

void ConfigurationStore::begin() {
  deserializeJson(current_, kDefaultConfig);

  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return;
  }
  const String serialized = preferences.getString(kConfigKey, "");
  preferences.end();

  JsonDocument document;
  JsonDocument loaded;
  if (deserializeJson(document, serialized) == DeserializationError::Ok &&
      parseConfig(document.as<JsonVariantConst>(), loaded)) {
    current_.set(loaded);
  }
}

const JsonDocument &ConfigurationStore::current() const {
  return current_;
}

bool ConfigurationStore::buildCandidate(JsonObjectConst request,
                                         JsonDocument &candidate,
                                         const char *&error) const {
  if (request.containsKey("config")) {
    if (request.containsKey("field") || request.containsKey("value") ||
        !parseConfig(request["config"], candidate)) {
      error = "invalid_config";
      return false;
    }
    return true;
  }

  const char *field = request["field"];
  if (!request.containsKey("value") || !candidate.set(current_) ||
      !setField(candidate, field, request["value"])) {
    error = "invalid_field_or_value";
    return false;
  }
  return true;
}

bool ConfigurationStore::save(const JsonDocument &candidate) const {
  String serialized;
  serializeJson(candidate, serialized);
  if (serialized.length() == 0 || candidate.overflowed()) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const size_t written = preferences.putString(kConfigKey, serialized);
  preferences.end();
  return written == serialized.length();
}

void ConfigurationStore::publish(const JsonDocument &candidate) {
  current_.set(candidate);
}

bool ConfigurationStore::getAcquisitionConfig(
    const JsonDocument &source, AcquisitionConfig &result) const {
  if (!source["sample_rate"].is<int32_t>() ||
      !source["min_voltage"].is<int32_t>() ||
      !source["max_voltage"].is<int32_t>()) {
    return false;
  }

  result.sample_rate = source["sample_rate"].as<int32_t>();
  result.min_voltage = source["min_voltage"].as<int32_t>();
  result.max_voltage = source["max_voltage"].as<int32_t>();
  JsonVariantConst shunt = source["shunt_res"];
  result.shunt_auto = shunt.is<const char *>();
  result.shunt_res = result.shunt_auto ? 0 : shunt.as<uint32_t>();
  return validValue(FieldType::ShuntResistance, shunt);
}

bool ConfigurationStore::hasField(const char *name) const {
  return findField(name) != nullptr;
}

bool ConfigurationStore::validValue(FieldType type,
                                    JsonVariantConst value) const {
  switch (type) {
    case FieldType::Int32:
      return value.is<int32_t>();
    case FieldType::Bool:
      return value.is<bool>();
    case FieldType::String:
      return value.is<const char *>();
    case FieldType::ShuntResistance:
      if (value.is<const char *>()) {
        return strcmp(value.as<const char *>(), "auto") == 0;
      }
      if (!value.is<uint32_t>()) {
        return false;
      }
      switch (value.as<uint32_t>()) {
        case 200:
        case 1000:
        case 10000:
        case 100000:
        case 1000000:
        case 10000000:
          return true;
        default:
          return false;
      }
  }
  return false;
}

bool ConfigurationStore::setField(JsonDocument &target, const char *name,
                                  JsonVariantConst value) const {
  const FieldDefinition *field = findField(name);
  if (field == nullptr || !validValue(field->type, value)) {
    return false;
  }
  target[name].set(value);
  return !target.overflowed();
}

bool ConfigurationStore::parseConfig(JsonVariantConst input,
                                     JsonDocument &result) const {
  if (!input.is<JsonObjectConst>()) {
    return false;
  }
  JsonObjectConst object = input.as<JsonObjectConst>();
  if (object.size() != kFieldCount) {
    return false;
  }
  for (const FieldDefinition &field : kFields) {
    if (!validValue(field.type, object[field.name])) {
      return false;
    }
  }
  return result.set(input) && !result.overflowed();
}

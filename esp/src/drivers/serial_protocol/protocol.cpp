#include "protocol.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

#include "config_generated.h"

namespace {

constexpr uint32_t kBaudRate = 115200;
constexpr size_t kMaxLineLength = 511;
constexpr char kNamespace[] = "lengmur";
constexpr char kConfigKey[] = "config";

JsonDocument config;
char inputLine[kMaxLineLength + 1];
size_t inputLength = 0;
bool discardLongLine = false;

const FieldDefinition *findField(const char *name) {
  if (name == nullptr) {
    return nullptr;
  }
  for (const FieldDefinition &field : kFields) {
    if (strcmp(name, field.name) == 0) {
      return &field;
    }
  }
  return nullptr;
}

bool validValue(FieldType type, JsonVariantConst value) {
  switch (type) {
    case FieldType::Int32:
      return value.is<int32_t>();
    case FieldType::Bool:
      return value.is<bool>();
    case FieldType::String:
      return value.is<const char *>();
  }
  return false;
}

bool setField(JsonDocument &target, const char *name, JsonVariantConst value) {
  const FieldDefinition *field = findField(name);
  if (field == nullptr || !validValue(field->type, value)) {
    return false;
  }
  target[name].set(value);
  return !target.overflowed();
}

bool parseConfig(JsonVariantConst input, JsonDocument &result) {
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

bool saveConfig(const JsonDocument &value) {
  String serialized;
  serializeJson(value, serialized);
  if (serialized.length() == 0 || value.overflowed()) {
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

void loadConfig() {
  deserializeJson(config, kDefaultConfig);

  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return;
  }
  const String serialized = preferences.getString(kConfigKey, "");
  preferences.end();

  JsonDocument document;
  if (deserializeJson(document, serialized) == DeserializationError::Ok) {
    JsonDocument loaded;
    if (parseConfig(document.as<JsonVariantConst>(), loaded)) {
      config.set(loaded);
    }
  }
}

void sendError(const char *message) {
  JsonDocument response;
  response["ok"] = false;
  response["error"] = message;
  serializeJson(response, Serial);
  Serial.println();
}

void sendSuccess() {
  Serial.println(F("{\"ok\":true}"));
}

void handleLine(const char *line) {
  JsonDocument request;
  if (deserializeJson(request, line) != DeserializationError::Ok ||
      !request.is<JsonObject>()) {
    sendError("invalid_json");
    return;
  }

  const char *operation = request["op"];
  if (operation == nullptr) {
    sendError("invalid_operation");
    return;
  }

  if (strcmp(operation, "set") == 0) {
    JsonDocument candidate;
    if (request.containsKey("config")) {
      if (request.containsKey("field") || request.containsKey("value") ||
          !parseConfig(request["config"].as<JsonVariantConst>(), candidate)) {
        sendError("invalid_config");
        return;
      }
    } else {
      const char *field = request["field"];
      if (!request.containsKey("value") || !candidate.set(config) ||
          !setField(candidate, field, request["value"].as<JsonVariantConst>())) {
        sendError("invalid_field_or_value");
        return;
      }
    }

    if (!saveConfig(candidate)) {
      sendError("storage_error");
      return;
    }
    config.set(candidate);
    sendSuccess();
    return;
  }

  if (strcmp(operation, "get") == 0) {
    JsonDocument response;
    response["ok"] = true;
    if (request.containsKey("field")) {
      const char *field = request["field"];
      if (findField(field) == nullptr) {
        sendError("unknown_field");
        return;
      }
      response["field"] = field;
      response["value"].set(config[field]);
    } else {
      response["config"].set(config);
    }
    serializeJson(response, Serial);
    Serial.println();
    return;
  }

  sendError("invalid_operation");
}

}  // namespace

void protocolBegin() {
  Serial.begin(kBaudRate);
  loadConfig();
}

void protocolPoll() {
  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\n') {
      if (discardLongLine) {
        sendError("line_too_long");
      } else if (inputLength > 0) {
        inputLine[inputLength] = '\0';
        handleLine(inputLine);
      }
      inputLength = 0;
      discardLongLine = false;
    } else if (character != '\r' && !discardLongLine) {
      if (inputLength < kMaxLineLength) {
        inputLine[inputLength++] = character;
      } else {
        discardLongLine = true;
      }
    }
  }
}

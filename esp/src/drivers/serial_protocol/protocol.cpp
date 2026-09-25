#include "protocol.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

#include "../../acquisition/acquisition.h"
#include "../output_control/output_control.h"
#include "config_generated.h"

namespace {

constexpr uint32_t kBaudRate = 115200;
constexpr uint8_t kMagic0 = 0x4c;  // L
constexpr uint8_t kMagic1 = 0x53;  // S
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kTypeCommand = 0;
constexpr uint8_t kTypeResponse = 1;
constexpr size_t kHeaderSize = 5;
constexpr size_t kCrcSize = 4;
constexpr size_t kMaxPayloadSize = 2048;
constexpr char kNamespace[] = "lengmur";
constexpr char kConfigKey[] = "config";

JsonDocument config;
uint8_t receiveBuffer[kHeaderSize + kMaxPayloadSize + kCrcSize];
size_t receiveLength = 0;

void writeU32(uint8_t *target, uint32_t value) {
  target[0] = static_cast<uint8_t>(value);
  target[1] = static_cast<uint8_t>(value >> 8);
  target[2] = static_cast<uint8_t>(value >> 16);
  target[3] = static_cast<uint8_t>(value >> 24);
}

uint16_t readU16(const uint8_t *source) {
  return static_cast<uint16_t>(source[0]) |
         (static_cast<uint16_t>(source[1]) << 8);
}

uint32_t readU32(const uint8_t *source) {
  return static_cast<uint32_t>(source[0]) |
         (static_cast<uint32_t>(source[1]) << 8) |
         (static_cast<uint32_t>(source[2]) << 16) |
         (static_cast<uint32_t>(source[3]) << 24);
}

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return crc;
}

uint32_t crc32(const uint8_t *data, size_t length) {
  return ~updateCrc32(0xffffffffu, data, length);
}

bool sendFrame(uint8_t type, const uint8_t *payload, size_t payloadSize) {
  if (type > 7 || payloadSize > UINT16_MAX) {
    return false;
  }
  uint8_t header[kHeaderSize] = {
      kMagic0,
      kMagic1,
      static_cast<uint8_t>((type << 5) | kProtocolVersion),
      static_cast<uint8_t>(payloadSize),
      static_cast<uint8_t>(payloadSize >> 8),
  };
  uint32_t crc = updateCrc32(0xffffffffu, header, sizeof(header));
  crc = ~updateCrc32(crc, payload, payloadSize);
  uint8_t encodedCrc[kCrcSize];
  writeU32(encodedCrc, crc);
  return Serial.write(header, sizeof(header)) == sizeof(header) &&
         Serial.write(payload, payloadSize) == payloadSize &&
         Serial.write(encodedCrc, sizeof(encodedCrc)) == sizeof(encodedCrc);
}

bool sendJson(uint8_t type, const JsonDocument &document) {
  const size_t size = measureJson(document);
  if (size > kMaxPayloadSize) {
    return false;
  }
  uint8_t payload[kMaxPayloadSize];
  return serializeJson(document, payload, sizeof(payload)) == size &&
         sendFrame(type, payload, size);
}

void sendError(const char *message) {
  JsonDocument response;
  response["ok"] = false;
  response["error"] = message;
  sendJson(kTypeResponse, response);
}

const FieldDefinition *findField(const char *name) {
  if (name != nullptr) {
    for (const FieldDefinition &field : kFields) {
      if (strcmp(name, field.name) == 0) return &field;
    }
  }
  return nullptr;
}

bool validValue(FieldType type, JsonVariantConst value) {
  switch (type) {
    case FieldType::Int32: return value.is<int32_t>();
    case FieldType::Bool: return value.is<bool>();
    case FieldType::String: return value.is<const char *>();
    case FieldType::ShuntResistance:
      if (value.is<const char *>()) return strcmp(value.as<const char *>(), "auto") == 0;
      if (!value.is<uint32_t>()) return false;
      switch (value.as<uint32_t>()) {
        case 200: case 1000: case 10000: case 100000: case 1000000: case 10000000:
          return true;
        default: return false;
      }
  }
  return false;
}

bool setField(JsonDocument &target, const char *name, JsonVariantConst value) {
  const FieldDefinition *field = findField(name);
  if (field == nullptr || !validValue(field->type, value)) return false;
  target[name].set(value);
  return !target.overflowed();
}

bool parseConfig(JsonVariantConst input, JsonDocument &result) {
  if (!input.is<JsonObjectConst>()) return false;
  JsonObjectConst object = input.as<JsonObjectConst>();
  if (object.size() != kFieldCount) return false;
  for (const FieldDefinition &field : kFields) {
    if (!validValue(field.type, object[field.name])) return false;
  }
  return result.set(input) && !result.overflowed();
}

bool saveConfig(const JsonDocument &value) {
  String serialized;
  serializeJson(value, serialized);
  if (serialized.length() == 0 || value.overflowed()) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const size_t written = preferences.putString(kConfigKey, serialized);
  preferences.end();
  return written == serialized.length();
}

void loadConfig() {
  deserializeJson(config, kDefaultConfig);
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return;
  const String serialized = preferences.getString(kConfigKey, "");
  preferences.end();
  JsonDocument document;
  JsonDocument loaded;
  if (deserializeJson(document, serialized) == DeserializationError::Ok &&
      parseConfig(document.as<JsonVariantConst>(), loaded)) {
    config.set(loaded);
  }
}

void handleCommand(const uint8_t *payload, size_t payloadSize) {
  JsonDocument request;
  if (deserializeJson(request, payload, payloadSize) != DeserializationError::Ok ||
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
        sendError("invalid_config"); return;
      }
    } else {
      const char *field = request["field"];
      if (!request.containsKey("value") || !candidate.set(config) ||
          !setField(candidate, field, request["value"].as<JsonVariantConst>())) {
        sendError("invalid_field_or_value"); return;
      }
    }
    if (!saveConfig(candidate)) { sendError("storage_error"); return; }
    config.set(candidate);
    JsonDocument response;
    response["ok"] = true;
    sendJson(kTypeResponse, response);
    return;
  }

  if (strcmp(operation, "get") == 0) {
    JsonDocument response;
    response["ok"] = true;
    if (request.containsKey("field")) {
      const char *field = request["field"];
      if (findField(field) == nullptr) { sendError("unknown_field"); return; }
      response["field"] = field;
      response["value"].set(config[field]);
    } else {
      response["config"].set(config);
    }
    sendJson(kTypeResponse, response);
    return;
  }

  if (strcmp(operation, "measure_once") == 0) {
    AcquisitionSample sample{};
    if (!acquisitionMeasureOnce(sample)) {
      sendError("measurement_failed");
      return;
    }
    JsonDocument response;
    response["ok"] = true;
    response["sample"]["voltage_raw"] = sample.voltage_raw;
    response["sample"]["current_raw"] = sample.current_raw;
    response["sample"]["shunt_res"] = sample.shunt_res;
    sendJson(kTypeResponse, response);
    return;
  }

  if (strcmp(operation, "get_out_state") == 0 ||
      strcmp(operation, "toggle_out") == 0) {
    const bool enabled = strcmp(operation, "toggle_out") == 0
                             ? outputControlToggle()
                             : outputControlIsEnabled();
    JsonDocument response;
    response["ok"] = true;
    response["out_enabled"] = enabled;
    sendJson(kTypeResponse, response);
    return;
  }
  sendError("invalid_operation");
}

void discardFirstByte() {
  if (receiveLength > 0) {
    memmove(receiveBuffer, receiveBuffer + 1, --receiveLength);
  }
}

void processReceiveBuffer() {
  while (receiveLength >= 2) {
    if (receiveBuffer[0] != kMagic0 || receiveBuffer[1] != kMagic1) {
      discardFirstByte();
      continue;
    }
    if (receiveLength < kHeaderSize) return;
    const uint8_t typeAndVersion = receiveBuffer[2];
    const uint8_t type = typeAndVersion >> 5;
    const uint8_t version = typeAndVersion & 0x1f;
    const size_t payloadSize = readU16(receiveBuffer + 3);
    if (payloadSize > kMaxPayloadSize) {
      discardFirstByte();
      continue;
    }
    const size_t frameSize = kHeaderSize + payloadSize + kCrcSize;
    if (receiveLength < frameSize) return;
    const uint32_t expectedCrc = readU32(receiveBuffer + kHeaderSize + payloadSize);
    if (crc32(receiveBuffer, kHeaderSize + payloadSize) != expectedCrc) {
      discardFirstByte();
      continue;
    }
    if (version != kProtocolVersion) {
      sendError("unsupported_version");
    } else if (type != kTypeCommand) {
      sendError("unexpected_packet_type");
    } else {
      handleCommand(receiveBuffer + kHeaderSize, payloadSize);
    }
    receiveLength -= frameSize;
    memmove(receiveBuffer, receiveBuffer + frameSize, receiveLength);
  }
}

}  // namespace

void protocolBegin() {
  Serial.begin(kBaudRate);
  loadConfig();
}

void protocolPoll() {
  while (Serial.available() > 0) {
    if (receiveLength == sizeof(receiveBuffer)) discardFirstByte();
    receiveBuffer[receiveLength++] = static_cast<uint8_t>(Serial.read());
  }
  processReceiveBuffer();
}

bool protocolGetAcquisitionConfig(AcquisitionConfig &result) {
  if (!config["sample_rate"].is<int32_t>() || !config["min_voltage"].is<int32_t>() ||
      !config["max_voltage"].is<int32_t>()) return false;
  result.sample_rate = config["sample_rate"].as<int32_t>();
  result.min_voltage = config["min_voltage"].as<int32_t>();
  result.max_voltage = config["max_voltage"].as<int32_t>();
  JsonVariantConst shunt = config["shunt_res"];
  result.shunt_auto = shunt.is<const char *>();
  result.shunt_res = result.shunt_auto ? 0 : shunt.as<uint32_t>();
  return validValue(FieldType::ShuntResistance, shunt);
}

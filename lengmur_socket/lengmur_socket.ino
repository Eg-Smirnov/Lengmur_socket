#include <ArduinoJson.h>
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

namespace {

constexpr uint32_t kBaudRate = 115200;
constexpr size_t kMaxLineLength = 511;
constexpr char kNamespace[] = "lengmur";
constexpr char kConfigKey[] = "config";

struct Config {
  int32_t setting_1 = 0;
  int32_t setting_2 = 0;
  int32_t setting_3 = 0;
};

Config config;
char inputLine[kMaxLineLength + 1];
size_t inputLength = 0;
bool discardLongLine = false;

void addConfig(JsonObject object, const Config &value) {
  object["setting_1"] = value.setting_1;
  object["setting_2"] = value.setting_2;
  object["setting_3"] = value.setting_3;
}

bool setField(Config &target, const char *name, JsonVariantConst value) {
  if (name == nullptr || !value.is<int32_t>()) {
    return false;
  }

  const int32_t number = value.as<int32_t>();
  if (strcmp(name, "setting_1") == 0) {
    target.setting_1 = number;
  } else if (strcmp(name, "setting_2") == 0) {
    target.setting_2 = number;
  } else if (strcmp(name, "setting_3") == 0) {
    target.setting_3 = number;
  } else {
    return false;
  }
  return true;
}

bool getField(const Config &source, const char *name, int32_t &value) {
  if (name == nullptr) {
    return false;
  }
  if (strcmp(name, "setting_1") == 0) {
    value = source.setting_1;
  } else if (strcmp(name, "setting_2") == 0) {
    value = source.setting_2;
  } else if (strcmp(name, "setting_3") == 0) {
    value = source.setting_3;
  } else {
    return false;
  }
  return true;
}

bool parseConfig(JsonVariantConst input, Config &result) {
  if (!input.is<JsonObjectConst>()) {
    return false;
  }

  const JsonObjectConst object = input.as<JsonObjectConst>();
  if (object.size() != 3) {
    return false;
  }

  Config candidate;
  if (!setField(candidate, "setting_1", object["setting_1"]) ||
      !setField(candidate, "setting_2", object["setting_2"]) ||
      !setField(candidate, "setting_3", object["setting_3"])) {
    return false;
  }
  result = candidate;
  return true;
}

bool saveConfig(const Config &value) {
  JsonDocument document;
  addConfig(document.to<JsonObject>(), value);

  String serialized;
  serializeJson(document, serialized);
  if (serialized.length() == 0 || document.overflowed()) {
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
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return;
  }
  const String serialized = preferences.getString(kConfigKey, "");
  preferences.end();

  JsonDocument document;
  if (deserializeJson(document, serialized) == DeserializationError::Ok) {
    Config loaded;
    if (parseConfig(document.as<JsonVariantConst>(), loaded)) {
      config = loaded;
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
    Config candidate = config;
    if (request.containsKey("config")) {
      if (request.containsKey("field") || request.containsKey("value") ||
          !parseConfig(request["config"].as<JsonVariantConst>(), candidate)) {
        sendError("invalid_config");
        return;
      }
    } else {
      const char *field = request["field"];
      if (!request.containsKey("value") ||
          !setField(candidate, field, request["value"].as<JsonVariantConst>())) {
        sendError("invalid_field_or_value");
        return;
      }
    }

    if (!saveConfig(candidate)) {
      sendError("storage_error");
      return;
    }
    config = candidate;
    sendSuccess();
    return;
  }

  if (strcmp(operation, "get") == 0) {
    JsonDocument response;
    response["ok"] = true;
    if (request.containsKey("field")) {
      const char *field = request["field"];
      int32_t value;
      if (!getField(config, field, value)) {
        sendError("unknown_field");
        return;
      }
      response["field"] = field;
      response["value"] = value;
    } else {
      addConfig(response.createNestedObject("config"), config);
    }
    serializeJson(response, Serial);
    Serial.println();
    return;
  }

  sendError("invalid_operation");
}

}  // namespace

void setup() {
  Serial.begin(kBaudRate);
  loadConfig();
}

void loop() {
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

#pragma once

#include <ArduinoJson.h>

#include "../../acquisition/acquisition.h"
#include "config_generated.h"

class ConfigurationStore {
 public:
  /** Loads a valid stored configuration or falls back to generated defaults. */
  void begin();

  /** Returns the currently published configuration. */
  const JsonDocument &current() const;

  /** Validates a `set` request and builds a complete candidate configuration. */
  bool buildCandidate(JsonObjectConst request, JsonDocument &candidate,
                      const char *&error) const;

  /** Writes a validated candidate to persistent storage. */
  bool save(const JsonDocument &candidate) const;

  /** Makes a successfully applied and stored candidate current. */
  void publish(const JsonDocument &candidate);

  /** Extracts acquisition settings from a validated JSON configuration. */
  bool getAcquisitionConfig(const JsonDocument &source,
                            AcquisitionConfig &result) const;

  /** Reports whether `name` belongs to the generated configuration schema. */
  bool hasField(const char *name) const;

 private:
  bool validValue(FieldType type, JsonVariantConst value) const;
  bool setField(JsonDocument &target, const char *name,
                JsonVariantConst value) const;
  bool parseConfig(JsonVariantConst input, JsonDocument &result) const;

  JsonDocument current_;
};

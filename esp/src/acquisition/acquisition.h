#pragma once

#include <stdint.h>

struct AcquisitionConfig {
  int32_t sample_rate;
  int32_t min_voltage;
  int32_t max_voltage;
  bool shunt_auto;
  uint32_t shunt_res;
};

/** Initializes the acquisition hardware and applies `configuration`. */
bool acquisitionBegin(const AcquisitionConfig &configuration);

/**
 * Applies `configuration` to the acquisition hardware.
 * Returns `true` when all affected hardware accepted the configuration.
 */
bool acquisitionApplyConfig(const AcquisitionConfig &configuration);

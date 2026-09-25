#pragma once

#include <stdint.h>

struct AcquisitionConfig {
  int32_t sample_rate;
  int32_t min_voltage;
  int32_t max_voltage;
  bool shunt_auto;
  uint32_t shunt_res;
};

/** Initializes the Serial transport and loads the stored configuration. */
void protocolBegin();

/** Receives and processes available Serial protocol data. */
void protocolPoll();

/**
 * Reads the current acquisition settings into `result`.
 * Returns `true` if the stored settings are valid.
 */
bool protocolGetAcquisitionConfig(AcquisitionConfig &result);

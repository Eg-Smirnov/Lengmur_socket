#pragma once

#include <stdint.h>

struct AcquisitionSample {
  int16_t voltage_raw;
  int16_t current_raw;
  uint32_t shunt_res;
};

/** Initializes the acquisition drivers and applies the current configuration. */
void acquisitionBegin();

/**
 * Applies the current acquisition configuration to the hardware.
 * Returns `true` if the configuration was valid and applied successfully.
 */
bool acquisitionApplyConfig();

/**
 * Reads one raw voltage and current sample using the configured shunt.
 * Stores the raw ADC values and selected shunt resistance in `sample`.
 * Returns `true` if the measurement completed successfully.
 */
bool acquisitionMeasureOnce(AcquisitionSample &sample);

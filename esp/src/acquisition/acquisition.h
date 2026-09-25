#pragma once

#include <stdint.h>

struct AcquisitionSample {
  int16_t voltage_raw;
  int16_t current_raw;
  uint32_t shunt_res;
};

void acquisitionBegin();
bool acquisitionApplyConfig();
bool acquisitionMeasureOnce(AcquisitionSample &sample);

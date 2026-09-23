#pragma once

#include <stdint.h>

struct AcquisitionConfig {
  int32_t sample_rate;
  int32_t min_voltage;
  int32_t max_voltage;
  bool shunt_auto;
  uint32_t shunt_res;
};

void protocolBegin();
void protocolPoll();
bool protocolGetAcquisitionConfig(AcquisitionConfig &result);

#include "acquisition.h"

#include "../drivers/ad7322/ad7322_pair.h"
#include "../drivers/adg5408/adg5408.h"
#include "../drivers/serial_protocol/protocol.h"

namespace {

AcquisitionConfig configuration;
ad7322::Pair adc_pair;
adg5408::ShuntSelector shunt_selector;

}  // namespace

void acquisitionBegin() {
  protocolGetAcquisitionConfig(configuration);
  shunt_selector.begin();
  adc_pair.begin();
}

bool acquisitionMeasureOnce(AcquisitionSample &sample) {
  if (!protocolGetAcquisitionConfig(configuration)) {
    return false;
  }

  const uint32_t requested_shunt = configuration.shunt_auto
                                       ? adg5408::ShuntSelector::k10MOhm
                                       : configuration.shunt_res;
  const bool shunt_changed = requested_shunt != shunt_selector.selectedOhms();
  if (!shunt_selector.selectOhms(requested_shunt)) {
    return false;
  }

  ad7322::SamplePair raw{};
  if (shunt_changed && !adc_pair.read(raw)) {
    return false;
  }
  if (!adc_pair.read(raw)) {
    return false;
  }

  sample = {raw.voltage, raw.current, shunt_selector.selectedOhms()};
  return true;
}

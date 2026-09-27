#include "acquisition.h"

#include "../drivers/adg5408/adg5408.h"

namespace {

adg5408::ShuntSelector shunt_selector;

}  // namespace

bool acquisitionBegin(const AcquisitionConfig &configuration) {
  shunt_selector.begin();
  return acquisitionApplyConfig(configuration);
}

bool acquisitionApplyConfig(const AcquisitionConfig &configuration) {
  const uint32_t requested_shunt = configuration.shunt_auto
                                       ? adg5408::ShuntSelector::k10MOhm
                                       : configuration.shunt_res;
  return shunt_selector.selectOhms(requested_shunt);
}

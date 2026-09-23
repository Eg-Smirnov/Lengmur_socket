#include "acquisition.h"

#include "../drivers/serial_protocol/protocol.h"

namespace {

AcquisitionConfig configuration;

}  // namespace

void acquisitionBegin() {
  // Data collection will be added here. Reading the snapshot here keeps the
  // configuration boundary explicit and verifies that all fields are visible.
  protocolGetAcquisitionConfig(configuration);
}

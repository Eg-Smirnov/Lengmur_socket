#include "src/drivers/serial_protocol/protocol.h"
#include "src/acquisition/acquisition.h"
#include "src/drivers/output_control/output_control.h"

void setup() {
  outputControlBegin();
  protocolBegin();
  acquisitionBegin();
}

void loop() {
  protocolPoll();
}

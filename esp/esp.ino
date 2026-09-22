#include "src/drivers/serial_protocol/protocol.h"

void setup() {
  protocolBegin();
}

void loop() {
  protocolPoll();
}

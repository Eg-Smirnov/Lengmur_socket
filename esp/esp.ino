#include "src/drivers/serial_protocol/protocol.h"
#include "src/acquisition/acquisition.h"

void setup() {
  protocolBegin();
  acquisitionBegin();
}

void loop() {
  protocolPoll();
}

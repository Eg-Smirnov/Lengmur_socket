#include "protocol.h"

void setup() {
  protocolBegin();
}

void loop() {
  protocolPoll();
}

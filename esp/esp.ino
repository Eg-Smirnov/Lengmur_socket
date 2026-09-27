#include "src/application/application.h"

Application application;

void setup() {
  application.begin();
}

void loop() {
  application.process();
}

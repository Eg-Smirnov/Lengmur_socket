#include "output_control.h"

#include <Arduino.h>

#include "../pins.h"

namespace {

bool enabled = false;

void applyState() {
  digitalWrite(pins::OUT_EN, enabled ? HIGH : LOW);
}

}  // namespace

void outputControlBegin() {
  enabled = false;
  digitalWrite(pins::OUT_EN, LOW);
  pinMode(pins::OUT_EN, OUTPUT);
}

bool outputControlIsEnabled() {
  return enabled;
}

bool outputControlToggle() {
  enabled = !enabled;
  applyState();
  return enabled;
}

#include "adg5408.h"

#include <Arduino.h>

#include "../pins.h"

namespace adg5408 {

bool ShuntSelector::addressForOhms(uint32_t ohms, uint8_t &address) {
  switch (ohms) {
    case k200Ohm: address = 0b000; return true;  // S1
    case k1KOhm: address = 0b001; return true;   // S2
    case k10KOhm: address = 0b010; return true;  // S3
    case k100KOhm: address = 0b011; return true; // S4
    case k1MOhm: address = 0b100; return true;   // S5
    case k10MOhm: address = 0b101; return true;  // S6; S7/S8 share resistor.
    default: return false;
  }
}

void ShuntSelector::writeAddress(uint8_t address) {
  // A0, A1, A2 correspond to RES_SEL_0, RES_SEL_1, RES_SEL_2.
  digitalWrite(pins::RES_SEL_0, (address & 0b001) ? HIGH : LOW);
  digitalWrite(pins::RES_SEL_1, (address & 0b010) ? HIGH : LOW);
  digitalWrite(pins::RES_SEL_2, (address & 0b100) ? HIGH : LOW);
}

void ShuntSelector::begin() {
  if (initialized_) {
    return;
  }

  // Preload GPIO output latches before enabling the pins as outputs.
  writeAddress(0b101);
  pinMode(pins::RES_SEL_0, OUTPUT);
  pinMode(pins::RES_SEL_1, OUTPUT);
  pinMode(pins::RES_SEL_2, OUTPUT);
  writeAddress(0b101);

  selected_ohms_ = k10MOhm;
  initialized_ = true;
}

bool ShuntSelector::selectOhms(uint32_t ohms) {
  uint8_t address = 0;
  if (!initialized_ || !addressForOhms(ohms, address)) {
    return false;
  }
  if (ohms != selected_ohms_) {
    writeAddress(address);
    selected_ohms_ = ohms;
  }
  return true;
}

}  // namespace adg5408

#pragma once

#include <stdint.h>

namespace adg5408 {

// Nominal resistor values; actual measurement scaling requires calibration.
class ShuntSelector {
 public:
  static constexpr uint32_t k200Ohm = 200;
  static constexpr uint32_t k1KOhm = 1000;
  static constexpr uint32_t k10KOhm = 10000;
  static constexpr uint32_t k100KOhm = 100000;
  static constexpr uint32_t k1MOhm = 1000000;
  static constexpr uint32_t k10MOhm = 10000000;

  // Select S6 (10 MOhm) as soon as the ESP32 can control the address pins.
  void begin();

  // Accepts only the six nominal values above. Invalid values leave GPIOs
  // unchanged and return false. Must not be called concurrently or from ISR.
  bool selectOhms(uint32_t ohms);

  uint32_t selectedOhms() const { return selected_ohms_; }
  bool initialized() const { return initialized_; }

 private:
  static bool addressForOhms(uint32_t ohms, uint8_t &address);
  void writeAddress(uint8_t address);

  uint32_t selected_ohms_ = 0;
  bool initialized_ = false;
};

}  // namespace adg5408

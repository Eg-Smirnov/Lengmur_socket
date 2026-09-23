#pragma once

#include <Arduino.h>
#include <SPI.h>

namespace ad7322 {

struct SamplePair {
  int16_t voltage;
  int16_t current;
  // Optional timestamp placeholder, currently disabled:
  // uint64_t timestamp_us;
};

// Owns both SPI controllers. Call only from one acquisition task; not ISR-safe.
class Pair {
 public:
  Pair();

  // Configures both ADCs for VIN0 - VIN1, +/-10 V, internal reference,
  // two's-complement output and normal power mode. Discards initial data.
  void begin();
  void end();

  // Starts both conversions before clocking either ADC. Returns false if an
  // ADC reports an unexpected leading/channel bit or begin() was not called.
  bool read(SamplePair &sample);

 private:
  uint16_t transfer(SPIClass &spi, uint8_t cs_pin, uint16_t command);
  static bool decode(uint16_t frame, int16_t &value);

  SPIClass voltage_spi_;
  SPIClass current_spi_;
  bool started_ = false;
};

}  // namespace ad7322

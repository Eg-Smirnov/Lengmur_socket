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

/** Controls the voltage and current AD7322 converters as a synchronized pair. */
class Pair {
 public:
  /** Constructs an uninitialized ADC pair. */
  Pair();

  /** Configures both ADCs for single-ended VIN0 relative to AGND, +/-10 V,
   * internal reference, two's-complement output and normal power mode.
   * Discards initial data.
   */
  void begin();

  /** Releases the SPI controllers used by both ADCs. */
  void end();

  /**
   * Reads one raw sample from each ADC into `sample`.
   * Returns `true` if the pair is initialized and both ADC frames are valid.
   */
  bool read(SamplePair &sample);

 private:
  /** Exchanges one frame with an ADC over the specified SPI bus. */
  uint16_t transfer(SPIClass &spi, uint8_t cs_pin, uint16_t command);

  /**
   * Sign-extends the 13-bit ADC code into the 16-bit integer `value`.
   * Does not convert the code to volts or amperes.
   * Returns `true` if the frame identifies the expected input channel.
   */
  static bool decode(uint16_t frame, int16_t &value);

  SPIClass voltage_spi_;
  SPIClass current_spi_;
  bool started_ = false;
};

}  // namespace ad7322

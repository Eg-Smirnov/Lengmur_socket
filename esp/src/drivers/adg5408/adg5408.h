#pragma once

#include <stdint.h>

namespace adg5408 {

/** Selects a nominal shunt resistance through the ADG5408 multiplexer. */
class ShuntSelector {
 public:
  static constexpr uint32_t k200Ohm = 200;
  static constexpr uint32_t k1KOhm = 1000;
  static constexpr uint32_t k10KOhm = 10000;
  static constexpr uint32_t k100KOhm = 100000;
  static constexpr uint32_t k1MOhm = 1000000;
  static constexpr uint32_t k10MOhm = 10000000;

  /** Initializes the address pins and selects the default 10 MOhm shunt. */
  void begin();

  /**
   * Selects the nominal shunt resistance specified by `ohms`.
   * Returns `true` if the selector is initialized and the value is supported.
   */
  bool selectOhms(uint32_t ohms);

  /** Returns the currently selected nominal resistance in ohms. */
  uint32_t selectedOhms() const { return selected_ohms_; }

  /** Reports whether the selector has been initialized. */
  bool initialized() const { return initialized_; }

 private:
  /** Maps a supported resistance to an ADG5408 channel address. */
  static bool addressForOhms(uint32_t ohms, uint8_t &address);

  /** Sets the ADG5408 address pins to the specified channel address. */
  void writeAddress(uint8_t address);

  uint32_t selected_ohms_ = 0;
  bool initialized_ = false;
};

}  // namespace adg5408

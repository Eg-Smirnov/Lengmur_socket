#include "ad7322_pair.h"

// Optional timestamp support, currently disabled:
// #include <esp_timer.h>

#include "../pins.h"

namespace ad7322 {
namespace {

constexpr uint32_t kSpiClockHz = 2000000;
const SPISettings kSpiSettings(kSpiClockHz, MSBFIRST, SPI_MODE0);

// AD7322 register addresses and fields (datasheet Rev. B, Tables 8-13).
constexpr uint16_t kRangeRegisterPm10V = 0xA000;  // VIN0/VIN1: +/-10 V.
constexpr uint16_t kControlPseudoDifferential =
    0x8000 | (1U << 8) | (1U << 4);  // MODE0=1, REF=1, ADD0=0.

}  // namespace

Pair::Pair() : voltage_spi_(HSPI), current_spi_(VSPI) {}

uint16_t Pair::transfer(SPIClass &spi, uint8_t cs_pin, uint16_t command) {
  spi.beginTransaction(kSpiSettings);
  digitalWrite(cs_pin, LOW);
  const uint16_t frame = spi.transfer16(command);
  digitalWrite(cs_pin, HIGH);
  spi.endTransaction();
  return frame;
}

void Pair::begin() {
  if (started_) {
    return;
  }

  pinMode(pins::USPI_CS, OUTPUT);
  pinMode(pins::ISPI_CS, OUTPUT);
  digitalWrite(pins::USPI_CS, HIGH);
  digitalWrite(pins::ISPI_CS, HIGH);

  voltage_spi_.begin(pins::USPI_CLK, pins::USPI_MISO, pins::USPI_MOSI, -1);
  current_spi_.begin(pins::ISPI_CLK, pins::ISPI_MISO, pins::ISPI_MOSI, -1);

  // The range register must be written before the control register.
  transfer(voltage_spi_, pins::USPI_CS, kRangeRegisterPm10V);
  transfer(current_spi_, pins::ISPI_CS, kRangeRegisterPm10V);
  transfer(voltage_spi_, pins::USPI_CS, kControlPseudoDifferential);
  transfer(current_spi_, pins::ISPI_CS, kControlPseudoDifferential);

  delayMicroseconds(500);  // Internal reference start-up time.
  started_ = true;
  SamplePair ignored{};
  read(ignored);  // Discard the first conversion after configuration.
}

void Pair::end() {
  if (!started_) {
    return;
  }
  voltage_spi_.end();
  current_spi_.end();
  started_ = false;
}

bool Pair::decode(uint16_t frame, int16_t &value) {
  // Bits 15:14 must be zero; bit 13 identifies VIN0 (zero).
  if ((frame & 0xE000U) != 0) {
    return false;
  }
  const uint16_t signed13 = frame & 0x1FFFU;
  value = static_cast<int16_t>(signed13 | ((signed13 & 0x1000U) ? 0xE000U : 0));
  return true;
}

bool Pair::read(SamplePair &sample) {
  if (!started_) {
    return false;
  }

  voltage_spi_.beginTransaction(kSpiSettings);
  current_spi_.beginTransaction(kSpiSettings);

  // Optional timestamp measurement, currently disabled to keep SamplePair
  // minimal and avoid extra work between the two CS falling edges:
  // const uint64_t before_us = static_cast<uint64_t>(esp_timer_get_time());
  digitalWrite(pins::USPI_CS, LOW);
  digitalWrite(pins::ISPI_CS, LOW);
  // const uint64_t after_us = static_cast<uint64_t>(esp_timer_get_time());

  const uint16_t voltage_frame = voltage_spi_.transfer16(0);
  const uint16_t current_frame = current_spi_.transfer16(0);

  digitalWrite(pins::USPI_CS, HIGH);
  digitalWrite(pins::ISPI_CS, HIGH);
  current_spi_.endTransaction();
  voltage_spi_.endTransaction();

  int16_t voltage = 0;
  int16_t current = 0;
  if (!decode(voltage_frame, voltage) || !decode(current_frame, current)) {
    return false;
  }
  sample = {voltage, current};
  // To restore timestamps, also add timestamp_us to SamplePair and use:
  // sample.timestamp_us = (before_us + after_us) / 2;
  return true;
}

}  // namespace ad7322

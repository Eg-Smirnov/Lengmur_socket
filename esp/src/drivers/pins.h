#pragma once

#include <stdint.h>

// ESP32-WROOM-32 GPIO assignments from docs/hardware/pinout.md.
// Keep peripheral drivers dependent on these names instead of raw pin numbers.
// GPIO0, GPIO2, GPIO5, GPIO12 and GPIO15 are strapping pins; peripheral
// circuitry must not drive them to an invalid level while the ESP32 resets.
namespace pins {

// Synchronization input (SENSOR_VN / ADC1_CH3, input only).
static constexpr uint8_t SYNC = 39;

// Voltage ADC (AD7322BRUZ) SPI bus.
static constexpr uint8_t USPI_MISO = 34;
static constexpr uint8_t USPI_MOSI = 32;
static constexpr uint8_t USPI_CS = 33;
static constexpr uint8_t USPI_CLK = 14;

// Current ADC (AD7322BRUZ) SPI bus.
static constexpr uint8_t ISPI_MISO = 35;
static constexpr uint8_t ISPI_MOSI = 12;
static constexpr uint8_t ISPI_CS = 13;
static constexpr uint8_t ISPI_CLK = 15;

// Current measurement range selection.
static constexpr uint8_t RES_SEL_0 = 2;
static constexpr uint8_t RES_SEL_1 = 4;
static constexpr uint8_t RES_SEL_2 = 16;

// Power output control.
static constexpr uint8_t OUT_EN = 17;

// KSZ8081 Ethernet PHY (RMII).
static constexpr uint8_t ETH_RST = 5;
static constexpr uint8_t RMII_MDIO = 18;
static constexpr uint8_t RMII_TXD0 = 19;
static constexpr uint8_t RMII_TXEN = 21;
static constexpr uint8_t RMII_TXD1 = 22;
static constexpr uint8_t RMII_MDC = 23;
static constexpr uint8_t RMII_RXD0 = 25;
static constexpr uint8_t RMII_RXD1 = 26;
static constexpr uint8_t RMII_CRS_DV = 27;

// USB UART nets and boot-mode signal (names match the schematic).
static constexpr uint8_t USB_UART_TXD = 3;
static constexpr uint8_t USB_UART_RXD = 1;
static constexpr uint8_t ESP32_IO0 = 0;

}  // namespace pins

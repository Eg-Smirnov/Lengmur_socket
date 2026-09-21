# Аппаратная часть

Рабочие заметки по подключению ESP32-WROOM-32 и назначению сигналов на схеме. Документ можно дополнять по мере уточнения схемы, разводки платы и реализации прошивки.

Обозначения:

- `NC` — вывод модуля не подключён на плате;
- `—` — для вывода не указана именованная сеть;
- направления UART указаны относительно ESP32.

## Распиновка модуля ESP32-WROOM-32

| Pin модуля | ESP32              | Net на схеме   |
| ----------: | ------------------ | -------------- |
|           1 | GND                | GND            |
|           2 | VDD33              | +3.3V          |
|           3 | EN                 | `ESP32_EN`     |
|           4 | SENSOR_VP / GPIO36 | —              |
|           5 | SENSOR_VN / GPIO39 | `SYNC`         |
|           6 | GPIO34             | `USPI_MISO`    |
|           7 | GPIO35             | `ISPI_MISO`    |
|           8 | GPIO32             | `USPI_MOSI`    |
|           9 | GPIO33             | `USPI_CS`      |
|          10 | GPIO25             | `RMII_RXD0`    |
|          11 | GPIO26             | `RMII_RXD1`    |
|          12 | GPIO27             | `RMII_CRS_DV`  |
|          13 | GPIO14             | `USPI_CLK`     |
|          14 | GPIO12             | `ISPI_MOSI`    |
|          15 | GND                | GND            |
|          16 | GPIO13             | `ISPI_CS`      |
|       17–19 | SD2, SD3, CMD      | NC             |
|          20 | CLK                | NC             |
|          21 | SD0                | NC             |
|          22 | SD1                | NC             |
|          23 | GPIO15             | `ISPI_CLK`     |
|          24 | GPIO2              | `RES_SEL_0`    |
|          25 | GPIO0              | `ESP32_IO0`    |
|          26 | GPIO4              | `RES_SEL_1`    |
|          27 | GPIO16             | `RES_SEL_2`    |
|          28 | GPIO17             | `OUT_EN`       |
|          29 | GPIO5              | `ETH_RST`      |
|          30 | GPIO18             | `RMII_MDIO`    |
|          31 | GPIO19             | `RMII_TXD0`    |
|          32 | NC                 | —              |
|          33 | GPIO21             | `RMII_TXEN`    |
|          34 | RXD0 / GPIO3       | `USB_UART_TXD` |
|          35 | TXD0 / GPIO1       | `USB_UART_RXD` |
|          36 | GPIO22             | `RMII_TXD1`    |
|          37 | GPIO23             | `RMII_MDC`     |
|          38 | GND                | GND            |
|          39 | GND/EP             | GND            |

## Назначение GPIO

| GPIO          | Сигнал         | Назначение                                                                                                                |
| -------------: | -------------- | ------------------------------------------------------------------------------------------------------------------------- |
|      **GPIO39** | `SYNC`         | Аналоговый/синхронизирующий вход. `SENSOR_VN`, ADC1_CH3. Предположительно служит для синхронизации измерений с сетевой синусоидой |
|      **GPIO34** | `USPI_MISO`    | MISO SPI измерения **напряжения**, внешний **AD7322BRUZ**                                                                 |
|      **GPIO32** | `USPI_MOSI`    | MOSI того же AD7322 напряжения                                                                                            |
|      **GPIO33** | `USPI_CS`      | CS AD7322 напряжения                                                                                                      |
|      **GPIO14** | `USPI_CLK`     | CLK AD7322 напряжения                                                                                                     |
|      **GPIO35** | `ISPI_MISO`    | MISO SPI измерения **тока**, второй **AD7322BRUZ**                                                                        |
|      **GPIO12** | `ISPI_MOSI`    | MOSI AD7322 тока                                                                                                          |
|      **GPIO13** | `ISPI_CS`      | CS AD7322 тока                                                                                                            |
|      **GPIO15** | `ISPI_CLK`     | CLK AD7322 тока                                                                                                           |
|       **GPIO2** | `RES_SEL_0`    | A0 выбора диапазона измерения тока                                                                                        |
|       **GPIO4** | `RES_SEL_1`    | A1 выбора диапазона                                                                                                       |
|      **GPIO16** | `RES_SEL_2`    | A2 выбора диапазона                                                                                                       |
|      **GPIO17** | `OUT_EN`       | Управление выходным силовым трактом / разрешение выхода                                                                   |
| **GPIO3 RXD0**  | `USB_UART_TXD` | Приём ESP32 ← **CP2102-GMR**                                                                                              |
| **GPIO1 TXD0**  | `USB_UART_RXD` | Передача ESP32 → **CP2102-GMR**                                                                                           |
|       **GPIO5** | `ETH_RST`      | Reset Ethernet PHY **KSZ8081RNACA-TR**                                                                                    |
|      **GPIO18** | `RMII_MDIO`    | Management data KSZ8081                                                                                                   |
|      **GPIO23** | `RMII_MDC`     | Management clock KSZ8081                                                                                                  |
|      **GPIO25** | `RMII_RXD0`    | Ethernet RX data 0                                                                                                        |
|      **GPIO26** | `RMII_RXD1`    | Ethernet RX data 1                                                                                                        |
|      **GPIO27** | `RMII_CRS_DV`  | Ethernet carrier sense / RX data valid                                                                                    |
|      **GPIO19** | `RMII_TXD0`    | Ethernet TX data 0                                                                                                        |
|      **GPIO22** | `RMII_TXD1`    | Ethernet TX data 1                                                                                                        |
|      **GPIO21** | `RMII_TXEN`    | Ethernet transmit enable                                                                                                  |
|       **GPIO0** | `ESP32_IO0`    | Boot/прошивка ESP32                                                                                                       |
|      **GPIO36** | SENSOR_VP      | На рассматриваемом фрагменте схемы не подключён                                                                           |

## Функциональные группы

- Измерение напряжения: `USPI_MISO`, `USPI_MOSI`, `USPI_CS`, `USPI_CLK`.
- Измерение тока: `ISPI_MISO`, `ISPI_MOSI`, `ISPI_CS`, `ISPI_CLK`.
- Выбор диапазона тока: `RES_SEL_0`…`RES_SEL_2`.
- Ethernet RMII: `RMII_RXD0`, `RMII_RXD1`, `RMII_CRS_DV`, `RMII_TXD0`, `RMII_TXD1`, `RMII_TXEN`, `RMII_MDIO`, `RMII_MDC`, `ETH_RST`.
- USB UART и загрузка: `USB_UART_TXD`, `USB_UART_RXD`, `ESP32_IO0`, `ESP32_EN`.
- Управление силовым трактом: `OUT_EN`.

> [!CAUTION]
> GPIO0, GPIO2, GPIO5, GPIO12 и GPIO15 являются strapping pins ESP32. Уровни на них во время сброса влияют на режим загрузки и конфигурацию микросхемы; это нужно учитывать при инициализации периферии и изменении схемы.

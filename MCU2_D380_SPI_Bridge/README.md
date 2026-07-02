# MCU2 D380 SPI Temperature Bridge

## Overview

Firmware for the **STM32G070** secondary chip (MCU2) on the D380 board.

MCU2 acts as a two-way SPI bridge:

```
D380 Temp Sensor ──SPI1 master──► MCU2 (STM32G070) ──SPI2 slave──► MCU1 (main controller)
```

MCU2 polls the D380 temperature sensor every second, maintains a shadow
register with the latest reading, and responds to MCU1 read requests over a
simple 5-byte SPI slave protocol.

---

## Hardware Pin Assignments

| Signal        | MCU2 Pin | Direction | Notes                            |
|---------------|----------|-----------|----------------------------------|
| SPI1\_SCK     | PA5      | Output    | D380 sensor clock                |
| SPI1\_MISO    | PA6      | Input     | D380 sensor data out             |
| SPI1\_MOSI    | PA7      | Output    | D380 sensor data in              |
| D380\_CS      | PA4      | Output    | Active-low, software-controlled  |
| SPI2\_SCK     | PB13     | Input     | MCU1 clock                       |
| SPI2\_MISO    | PB14     | Output    | Temperature data to MCU1         |
| SPI2\_MOSI    | PB15     | Input     | Command from MCU1                |
| SPI2\_NSS     | PB12     | Input     | Hardware NSS from MCU1           |
| STATUS\_LED   | PC6      | Output    | 500 ms heartbeat blink           |

---

## SPI Slave Protocol (MCU2 ← MCU1)

MCU1 initiates a 6-byte SPI transaction:

| Byte   | MCU1 → MCU2 | MCU2 → MCU1     | Description              |
|--------|-------------|-----------------|--------------------------|
| 0      | `0xA5`      | `0x00` (dummy)  | Command: read temperature |
| 1      | `0xFF`      | `0x5A` (header) | Response start marker    |
| 2      | `0xFF`      | `tempX10 MSB`   | Temperature MSB          |
| 3      | `0xFF`      | `tempX10 LSB`   | Temperature LSB          |
| 4      | `0xFF`      | `status`        | Data freshness code      |
| 5      | `0xFF`      | `CRC8(1..4)`    | Dallas/Maxim CRC-8       |

**tempX10**: signed 16-bit big-endian integer, °C × 10 (e.g. `0x00F5` = 24.5 °C).

**Status codes**:

| Value | Meaning                                      |
|-------|----------------------------------------------|
| `0x00`| Data valid and fresh (< 2 s old)             |
| `0x01`| Data valid but stale (> 2 s since last read) |
| `0x02`| Sensor error — last known value returned     |
| `0x03`| No reading yet (power-on init)               |

---

## Directory Structure

```
MCU2_D380_SPI_Bridge/
├── Inc/
│   ├── main.h                      # Top-level header, Error_Handler
│   └── stm32g0xx_hal_conf.h        # HAL module selection
├── Src/
│   ├── config/
│   │   ├── bridgeConfig.h          # Compile-time constants
│   │   ├── bridgeConfig.c          # Runtime config initialiser
│   │   └── bridgeConfigTypes.h     # BridgeConfig struct definition
│   ├── drivers/
│   │   ├── spiMaster.h/.c          # SPI1 master driver (D380 sensor)
│   │   ├── spiSlave.h/.c           # SPI2 slave driver (MCU1 interface)
│   │   ├── tempSensorD380.h/.c     # D380 temperature sensor driver
│   ├── application/
│   │   └── tempBridge.h/.c         # Bridge state machine
│   └── utils/
│       └── crc8.h/.c               # CRC-8 (Dallas/Maxim polynomial 0x31)
└── main.c                          # Entry point + cooperative scheduler
```

---

## Build Instructions

1. Open (or create) a Keil MDK-ARM project targeting **STM32G070CBTx**.
2. Add all `.c` files from `Src/` and `main.c` to the project source group.
3. Add `Inc/` and `Src/config/` to the include path.
4. Link against the STM32G0xx HAL library (same version as MCU1 project).
5. Build and flash to the MCU2 chip.

---

## Configuration

All tunable parameters are centralised in `Src/config/bridgeConfig.h`.
Key constants:

| Constant                       | Default | Description                        |
|--------------------------------|---------|------------------------------------|
| `TASK_SENSOR_READ_PERIOD_MS`   | 1000    | D380 poll interval (ms)            |
| `TASK_SLAVE_UPDATE_PERIOD_MS`  | 100     | Shadow buffer refresh (ms)         |
| `D380_CONVERSION_TIMEOUT_MS`   | 800     | 12-bit conversion wait (ms)        |
| `D380_READ_RETRY_COUNT`        | 3       | Retries before ERROR state         |
| `SPI_MASTER_BAUDRATE_PRESCALER`| /8      | SPI1 baud rate divisor             |
| `IWDG_RELOAD_VALUE`            | 4000    | Watchdog reload (~4 s timeout)     |

---

## Version History

- **V1.0.0** (2026-07-02): Initial release — D380 SPI bridge firmware

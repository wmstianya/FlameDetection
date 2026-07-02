/**
  ******************************************************************************
  * @file           : bridgeConfig.h
  * @brief          : System configuration for MCU2 D380 SPI Temperature Bridge
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Centralized configuration for the MCU2 sub-chip firmware running on
  * STM32G070. This chip acts as an SPI bridge between the D380 temperature
  * sensor (SPI1 master) and the main MCU1 controller (SPI2 slave).
  *
  * Hardware pin assignments:
  *   SPI1 master (D380 sensor):
  *     PA5  = SPI1_SCK
  *     PA6  = SPI1_MISO
  *     PA7  = SPI1_MOSI
  *     PA4  = SPI1_CS  (software-controlled GPIO)
  *   SPI2 slave (to MCU1):
  *     PB13 = SPI2_SCK
  *     PB14 = SPI2_MISO
  *     PB15 = SPI2_MOSI
  *     PB12 = SPI2_NSS (hardware NSS)
  *   Status LED:
  *     PC6  = STATUS_LED_PIN
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __BRIDGE_CONFIG_H
#define __BRIDGE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================================
   SPI Master Configuration (SPI1 → D380 temperature sensor)
   ============================================================================ */

/** SPI1 clock prescaler: APB/8 → ~8 MHz at 64 MHz system clock */
#define SPI_MASTER_BAUDRATE_PRESCALER   SPI_BAUDRATEPRESCALER_8

/** Maximum time to wait for a SPI master transfer to complete (ms) */
#define SPI_MASTER_TIMEOUT_MS           10u

/** GPIO port/pin for D380 chip-select (active low) */
#define D380_CS_PORT                    GPIOA
#define D380_CS_PIN                     GPIO_PIN_4

/** Delay in microseconds between CS assert and first clock edge */
#define D380_CS_SETUP_US                2u

/** Delay in microseconds between last clock edge and CS de-assert */
#define D380_CS_HOLD_US                 2u

/* ============================================================================
   SPI Slave Configuration (SPI2 ← MCU1 master)
   ============================================================================ */

/** Maximum time to wait for a SPI slave transaction (ms).
 *  MCU1 polls at 100 ms intervals; 200 ms gives one full missed cycle margin. */
#define SPI_SLAVE_TIMEOUT_MS            200u

/** Size of the SPI slave TX/RX frame in bytes (4-byte float + 1-byte CRC8) */
#define SPI_SLAVE_FRAME_SIZE            5u

/** Command byte sent by MCU1 to request a temperature reading */
#define SPI_CMD_READ_TEMP               0xA5u

/** Response header byte prepended by MCU2 to each temperature packet */
#define SPI_RESP_HEADER                 0x5Au

/* ============================================================================
   D380 Temperature Sensor Register Map
   ============================================================================ */

/** D380 register: start a new temperature conversion */
#define D380_REG_CONVERT                0x44u

/** D380 register: read the temperature scratchpad (9 bytes) */
#define D380_REG_READ_SCRATCHPAD        0xBEu

/** D380 register: skip ROM match (broadcast command) */
#define D380_REG_SKIP_ROM               0xCCu

/** Number of retries for a failed D380 SPI read before returning an error */
#define D380_READ_RETRY_COUNT           3u

/** Timeout for a D380 temperature conversion to complete (ms).
 *  12-bit resolution requires up to 750 ms per the datasheet. */
#define D380_CONVERSION_TIMEOUT_MS      800u

/** Minimum valid temperature output from D380 (°C × 10) */
#define D380_TEMP_MIN_X10               (-550)   /* -55.0 °C */

/** Maximum valid temperature output from D380 (°C × 10) */
#define D380_TEMP_MAX_X10               (1250)   /* +125.0 °C */

/* ============================================================================
   Task Scheduler Timing (ms)
   ============================================================================ */

/** Period for reading D380 and updating the shadow register */
#define TASK_SENSOR_READ_PERIOD_MS      1000u

/** Period for refreshing the SPI slave TX buffer */
#define TASK_SLAVE_UPDATE_PERIOD_MS     100u

/** Period for watchdog kick */
#define TASK_WATCHDOG_PERIOD_MS         200u

/** Period for toggling the status LED */
#define TASK_STATUS_LED_PERIOD_MS       500u

/* ============================================================================
   IWDG Configuration
   ============================================================================ */

/** IWDG prescaler — with LSI ~32 kHz: timeout ≈ prescaler × reload / 32000 */
#define IWDG_PRESCALER_VALUE            IWDG_PRESCALER_32

/** IWDG reload value — gives ~4 s timeout at prescaler /32 */
#define IWDG_RELOAD_VALUE               4000u

/* ============================================================================
   GPIO Pin Definitions
   ============================================================================ */

#define STATUS_LED_PORT                 GPIOC
#define STATUS_LED_PIN                  GPIO_PIN_6

/* ============================================================================
   System Constants
   ============================================================================ */

/** Temperature scale factor: internal representation is °C × 10 */
#define TEMP_SCALE_FACTOR               10u

/** Default/fallback temperature returned when no valid reading is available */
#define TEMP_DEFAULT_X10                300     /* 30.0 °C */

/** CRC8 polynomial (Dallas/Maxim 1-Wire) */
#define CRC8_POLYNOMIAL                 0x31u

#ifdef __cplusplus
}
#endif

#endif /* __BRIDGE_CONFIG_H */

/************************ END OF FILE *****************************/

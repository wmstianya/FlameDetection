/**
  ******************************************************************************
  * @file           : tempSensorD380.h
  * @brief          : D380 SPI temperature sensor driver interface
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Provides a register-level driver for the D380 temperature sensor accessed
  * via SPI1 master (see spiMaster.h). The D380 uses a DS18B20-compatible
  * SPI framing: command byte → conversion wait → read scratchpad.
  *
  * Temperature is returned as integer °C × 10 to avoid floating-point
  * overhead in the ISR-safe read path.
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __TEMP_SENSOR_D380_H
#define __TEMP_SENSOR_D380_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../config/bridgeConfig.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Return codes for D380 sensor operations.
  */
typedef enum {
    D380_OK = 0,            /**< Read succeeded and value is valid */
    D380_BUSY,              /**< Conversion not yet complete */
    D380_TIMEOUT,           /**< Conversion or SPI transfer timed out */
    D380_CRC_ERROR,         /**< Scratchpad CRC8 mismatch */
    D380_RANGE_ERROR,       /**< Temperature outside ±125 °C D380 range */
    D380_SPI_ERROR,         /**< Underlying SPI1 transfer failed */
    D380_PARAM_ERROR        /**< NULL pointer or bad argument */
} D380Status;

/**
  * @brief  D380 sensor context — one instance per physical sensor.
  */
typedef struct {
    int16_t  lastTempX10;       /**< Last successfully read temp (°C × 10) */
    uint32_t lastReadTickMs;    /**< HAL tick of the last successful read */
    uint8_t  conversionPending; /**< Non-zero while a conversion is in flight */
    uint32_t convStartTickMs;   /**< HAL tick when conversion was requested */
    D380Status lastStatus;      /**< Status of the most recent operation */
} D380Sensor;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise the D380 sensor context to safe defaults.
  * @param  sensor  Non-NULL pointer to D380Sensor context.
  * @retval None
  */
void d380SensorInit(D380Sensor *sensor);

/**
  * @brief  Send a "start conversion" command to the D380.
  * @note   After calling this function, wait at least D380_CONVERSION_TIMEOUT_MS
  *         before calling d380ReadTemperature().
  * @param  sensor  Non-NULL pointer to D380Sensor context.
  * @retval D380Status
  */
D380Status d380StartConversion(D380Sensor *sensor);

/**
  * @brief  Read the D380 scratchpad and decode the temperature.
  * @note   Must be called after the conversion window has elapsed.
  * @param  sensor   Non-NULL pointer to D380Sensor context.
  * @param  tempX10  Output: temperature in °C × 10 (e.g. 253 = 25.3 °C).
  * @retval D380Status — D380_BUSY if conversion window has not elapsed.
  */
D380Status d380ReadTemperature(D380Sensor *sensor, int16_t *tempX10);

/**
  * @brief  Blocking convenience wrapper: start conversion, wait, then read.
  * @param  sensor   Non-NULL pointer to D380Sensor context.
  * @param  tempX10  Output: temperature in °C × 10.
  * @retval D380Status
  */
D380Status d380ReadTemperatureBlocking(D380Sensor *sensor, int16_t *tempX10);

#ifdef __cplusplus
}
#endif

#endif /* __TEMP_SENSOR_D380_H */

/************************ END OF FILE *****************************/

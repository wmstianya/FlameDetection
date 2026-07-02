/**
  ******************************************************************************
  * @file           : tempBridge.h
  * @brief          : Application-layer temperature bridge logic interface
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Coordinates the D380 sensor driver and the SPI slave driver:
  *   - Schedules non-blocking temperature conversions on the D380
  *   - Updates the SPI slave shadow register on each successful read
  *   - Tracks data staleness and propagates status codes to MCU1
  *   - Manages retry logic on sensor errors
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __TEMP_BRIDGE_H
#define __TEMP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../config/bridgeConfigTypes.h"
#include "../drivers/tempSensorD380.h"
#include "../drivers/spiSlave.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Overall bridge operational state.
  */
typedef enum {
    BRIDGE_STATE_IDLE = 0,      /**< Waiting for next conversion cycle */
    BRIDGE_STATE_CONVERTING,    /**< D380 conversion in progress */
    BRIDGE_STATE_READ_PENDING,  /**< Ready to read scratchpad */
    BRIDGE_STATE_ERROR          /**< Persistent sensor error */
} BridgeState;

/**
  * @brief  Bridge application context — one instance for the whole system.
  */
typedef struct {
    BridgeState    state;               /**< Current state-machine state */
    D380Sensor     sensor;              /**< D380 sensor driver context */
    int16_t        currentTempX10;      /**< Last valid temperature (°C × 10) */
    uint32_t       lastGoodReadMs;      /**< Tick of last successful read */
    uint8_t        consecutiveErrors;   /**< Running count of read failures */
    uint32_t       nextConversionMs;    /**< Scheduled tick for next conversion */
    const BridgeConfig *cfg;            /**< Pointer to runtime config */
} TempBridge;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise the temperature bridge context.
  * @param  bridge  Non-NULL pointer to TempBridge to initialise.
  * @param  cfg     Non-NULL pointer to runtime configuration.
  * @retval None
  */
void tempBridgeInit(TempBridge *bridge, const BridgeConfig *cfg);

/**
  * @brief  Sensor read cooperative task — call every scheduler cycle.
  * @note   Internally manages the IDLE → CONVERTING → READ_PENDING → IDLE
  *         state machine. Calls spiSlaveUpdateTemp() on successful reads.
  * @param  bridge  Non-NULL pointer to TempBridge context.
  * @param  nowMs   Current HAL tick value.
  * @retval None
  */
void tempBridgeTaskSensor(TempBridge *bridge, uint32_t nowMs);

/**
  * @brief  SPI slave polling task — call every scheduler cycle.
  * @param  bridge  Non-NULL pointer to TempBridge context.
  * @param  nowMs   Current HAL tick value.
  * @retval None
  */
void tempBridgeTaskSlave(TempBridge *bridge, uint32_t nowMs);

/**
  * @brief  Retrieve the current temperature stored in the bridge.
  * @param  bridge   Non-NULL pointer to TempBridge context.
  * @param  tempX10  Output: temperature in °C × 10.
  * @retval SpiSlaveDataStatus indicating data freshness.
  */
SpiSlaveDataStatus tempBridgeGetTemp(const TempBridge *bridge,
                                     int16_t          *tempX10);

#ifdef __cplusplus
}
#endif

#endif /* __TEMP_BRIDGE_H */

/************************ END OF FILE *****************************/

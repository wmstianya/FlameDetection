/**
  ******************************************************************************
  * @file           : bridgeConfigTypes.h
  * @brief          : Shared type definitions for the bridge configuration
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Separates struct/typedef declarations from macro constants so that modules
  * can include only the types they need without pulling in the full macro set.
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __BRIDGE_CONFIG_TYPES_H
#define __BRIDGE_CONFIG_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "bridgeConfig.h"

/**
  * @brief  Runtime-tunable bridge parameters.
  *         Populated from compile-time macros by bridgeConfigInit().
  */
typedef struct {
    uint32_t spiMasterTimeoutMs;    /**< SPI1 master transfer timeout (ms) */
    uint32_t spiSlaveTimeoutMs;     /**< SPI2 slave transaction timeout (ms) */
    uint8_t  d380ReadRetryCount;    /**< D380 read retry limit */
    uint32_t d380ConvTimeoutMs;     /**< D380 conversion completion timeout (ms) */
    uint32_t taskSensorPeriodMs;    /**< Sensor read task period (ms) */
    uint32_t taskSlaveUpdateMs;     /**< Slave TX-buffer refresh period (ms) */
    uint32_t taskWatchdogMs;        /**< Watchdog kick period (ms) */
    uint32_t taskStatusLedMs;       /**< Status LED toggle period (ms) */
    int16_t  tempDefaultX10;        /**< Fallback temperature (°C × 10) */
} BridgeConfig;

/* Prototype ----------------------------------------------------------------*/

/**
  * @brief  Initialise a BridgeConfig with compile-time defaults.
  * @param  cfg  Pointer to BridgeConfig to populate. Must not be NULL.
  * @retval None
  */
void bridgeConfigInit(BridgeConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __BRIDGE_CONFIG_TYPES_H */

/************************ END OF FILE *****************************/

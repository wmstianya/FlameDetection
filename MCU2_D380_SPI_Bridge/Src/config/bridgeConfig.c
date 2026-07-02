/**
  ******************************************************************************
  * @file           : bridgeConfig.c
  * @brief          : Runtime configuration initializer for MCU2 bridge
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Provides bridgeConfigInit() which populates a BridgeConfig structure with
  * compile-time defaults. A single instance of BridgeConfig should live in
  * main.c and be passed to each sub-module by pointer.
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#include "bridgeConfig.h"
#include "bridgeConfigTypes.h"

/**
  * @brief  Populate a BridgeConfig with compile-time defaults.
  * @param  cfg  Non-NULL pointer to the config structure to initialise.
  * @retval None
  */
void bridgeConfigInit(BridgeConfig *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->spiMasterTimeoutMs    = SPI_MASTER_TIMEOUT_MS;
    cfg->spiSlaveTimeoutMs     = SPI_SLAVE_TIMEOUT_MS;
    cfg->d380ReadRetryCount    = D380_READ_RETRY_COUNT;
    cfg->d380ConvTimeoutMs     = D380_CONVERSION_TIMEOUT_MS;
    cfg->taskSensorPeriodMs    = TASK_SENSOR_READ_PERIOD_MS;
    cfg->taskSlaveUpdateMs     = TASK_SLAVE_UPDATE_PERIOD_MS;
    cfg->taskWatchdogMs        = TASK_WATCHDOG_PERIOD_MS;
    cfg->taskStatusLedMs       = TASK_STATUS_LED_PERIOD_MS;
    cfg->tempDefaultX10        = TEMP_DEFAULT_X10;
}

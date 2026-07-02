/**
  ******************************************************************************
  * @file           : tempBridge.c
  * @brief          : Application-layer temperature bridge implementation
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Implements a three-state non-blocking loop:
  *
  *   IDLE ──────────── (timer expires) ──────────► CONVERTING
  *     ▲                                               │
  *     │                                     (conversion window elapses)
  *     │                                               ▼
  *     └──────── (read OK / max retries) ──── READ_PENDING
  *                                                     │
  *                                           (D380 CRC/range error)
  *                                                     ▼
  *                                                  ERROR ──► (reset after
  *                                                              cooling period)
  *
  * Data staleness thresholds (derived from task period):
  *   > 2 × TASK_SENSOR_READ_PERIOD_MS → SPI_SLAVE_DATA_STALE
  *   Persistent sensor error           → SPI_SLAVE_DATA_SENSOR_ERR
  *   No reading yet                    → SPI_SLAVE_DATA_INIT
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#include "tempBridge.h"
#include "../drivers/tempSensorD380.h"
#include "../drivers/spiSlave.h"

/* Private constants ---------------------------------------------------------*/

/** Readings more than this many ms old are reported as stale (2 cycles) */
#define STALE_THRESHOLD_FACTOR  2u

/** After this many consecutive errors, transition to ERROR state */
#define MAX_CONSECUTIVE_ERRORS  5u

/** Cooldown period in ERROR state before retrying (ms) */
#define ERROR_COOLDOWN_MS       5000u

/* Private helpers -----------------------------------------------------------*/

/**
  * @brief  Determine data freshness based on age of last good read.
  * @param  bridge  Bridge context.
  * @param  nowMs   Current tick.
  * @retval SpiSlaveDataStatus
  */
static SpiSlaveDataStatus assessDataFreshness(const TempBridge *bridge,
                                               uint32_t          nowMs)
{
    uint32_t staleThresholdMs;

    if (bridge->lastGoodReadMs == 0u) {
        return SPI_SLAVE_DATA_INIT;
    }

    if (bridge->state == BRIDGE_STATE_ERROR) {
        return SPI_SLAVE_DATA_SENSOR_ERR;
    }

    staleThresholdMs = bridge->cfg->taskSensorPeriodMs * STALE_THRESHOLD_FACTOR;

    if ((nowMs - bridge->lastGoodReadMs) > staleThresholdMs) {
        return SPI_SLAVE_DATA_STALE;
    }

    return SPI_SLAVE_DATA_VALID;
}

/**
  * @brief  Handle a successful D380 temperature read.
  * @param  bridge   Bridge context.
  * @param  tempX10  Temperature just read.
  * @param  nowMs    Current tick.
  */
static void handleReadSuccess(TempBridge *bridge,
                               int16_t     tempX10,
                               uint32_t    nowMs)
{
    bridge->currentTempX10      = tempX10;
    bridge->lastGoodReadMs      = nowMs;
    bridge->consecutiveErrors   = 0u;
    bridge->state               = BRIDGE_STATE_IDLE;
    bridge->nextConversionMs    = nowMs + bridge->cfg->taskSensorPeriodMs;

    spiSlaveUpdateTemp(tempX10, SPI_SLAVE_DATA_VALID);
}

/**
  * @brief  Handle a D380 read failure.
  * @param  bridge  Bridge context.
  * @param  nowMs   Current tick.
  */
static void handleReadFailure(TempBridge *bridge, uint32_t nowMs)
{
    bridge->consecutiveErrors++;

    if (bridge->consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
        bridge->state            = BRIDGE_STATE_ERROR;
        bridge->nextConversionMs = nowMs + ERROR_COOLDOWN_MS;
        spiSlaveUpdateTemp(bridge->currentTempX10, SPI_SLAVE_DATA_SENSOR_ERR);
        return;
    }

    /* Retry sooner than the normal period */
    bridge->state            = BRIDGE_STATE_IDLE;
    bridge->nextConversionMs = nowMs + (bridge->cfg->taskSensorPeriodMs / 4u);
    spiSlaveUpdateTemp(bridge->currentTempX10, SPI_SLAVE_DATA_STALE);
}

/**
  * @brief  Kick off a new D380 conversion (IDLE → CONVERTING).
  * @param  bridge  Bridge context.
  */
static void startNewConversion(TempBridge *bridge)
{
    D380Status d380Status = d380StartConversion(&bridge->sensor);

    if (d380Status == D380_OK) {
        bridge->state = BRIDGE_STATE_CONVERTING;
    } else {
        handleReadFailure(bridge, HAL_GetTick());
    }
}

/**
  * @brief  Poll for conversion completion (CONVERTING → READ_PENDING).
  * @param  bridge  Bridge context.
  * @param  nowMs   Current tick.
  */
static void pollConversionReady(TempBridge *bridge, uint32_t nowMs)
{
    uint32_t elapsed = nowMs - bridge->sensor.convStartTickMs;

    if (elapsed >= bridge->cfg->d380ConvTimeoutMs) {
        bridge->state = BRIDGE_STATE_READ_PENDING;
    }
}

/**
  * @brief  Read scratchpad from D380 (READ_PENDING → IDLE or ERROR).
  * @param  bridge  Bridge context.
  * @param  nowMs   Current tick.
  */
static void performScratchpadRead(TempBridge *bridge, uint32_t nowMs)
{
    int16_t    tempX10;
    D380Status d380Status;

    bridge->sensor.conversionPending = 0u;
    d380Status = d380ReadTemperature(&bridge->sensor, &tempX10);

    if (d380Status == D380_OK) {
        handleReadSuccess(bridge, tempX10, nowMs);
    } else {
        handleReadFailure(bridge, nowMs);
    }
}

/* Public API ----------------------------------------------------------------*/

/**
  * @brief  Initialise the bridge context with safe defaults.
  */
void tempBridgeInit(TempBridge *bridge, const BridgeConfig *cfg)
{
    if (bridge == NULL || cfg == NULL) { return; }

    bridge->cfg                = cfg;
    bridge->state              = BRIDGE_STATE_IDLE;
    bridge->currentTempX10     = cfg->tempDefaultX10;
    bridge->lastGoodReadMs     = 0u;
    bridge->consecutiveErrors  = 0u;
    bridge->nextConversionMs   = 0u;

    d380SensorInit(&bridge->sensor);
}

/**
  * @brief  Handle the ERROR state: re-arm IDLE after cooldown.
  * @param  bridge  Bridge context.
  * @param  nowMs   Current tick.
  */
static void handleErrorState(TempBridge *bridge, uint32_t nowMs)
{
    if (nowMs >= bridge->nextConversionMs) {
        bridge->state             = BRIDGE_STATE_IDLE;
        bridge->consecutiveErrors = 0u;
    }
}

/**
  * @brief  Sensor state-machine task — drives D380 read cycle.
  */
void tempBridgeTaskSensor(TempBridge *bridge, uint32_t nowMs)
{
    if (bridge == NULL) { return; }
    switch (bridge->state) {
        case BRIDGE_STATE_IDLE:
            if (nowMs >= bridge->nextConversionMs) { startNewConversion(bridge); }
            break;
        case BRIDGE_STATE_CONVERTING:
            pollConversionReady(bridge, nowMs);
            break;
        case BRIDGE_STATE_READ_PENDING:
            performScratchpadRead(bridge, nowMs);
            break;
        case BRIDGE_STATE_ERROR:
            handleErrorState(bridge, nowMs);
            break;
        default:
            bridge->state = BRIDGE_STATE_IDLE;
            break;
    }
}

/**
  * @brief  SPI slave polling task — services MCU1 read requests.
  */
void tempBridgeTaskSlave(TempBridge *bridge, uint32_t nowMs)
{
    SpiSlaveDataStatus freshness;

    if (bridge == NULL) { return; }

    freshness = assessDataFreshness(bridge, nowMs);
    spiSlaveUpdateTemp(bridge->currentTempX10, freshness);

    /* Non-blocking poll; timeout is expected when MCU1 is silent */
    spiSlavePollAndRespond();
}

/**
  * @brief  Get the current temperature and its freshness from the bridge.
  */
SpiSlaveDataStatus tempBridgeGetTemp(const TempBridge *bridge,
                                     int16_t          *tempX10)
{
    SpiSlaveDataStatus freshness;

    if (bridge == NULL || tempX10 == NULL) {
        return SPI_SLAVE_DATA_INIT;
    }

    freshness  = assessDataFreshness(bridge, HAL_GetTick());
    *tempX10   = bridge->currentTempX10;
    return freshness;
}

/************************ END OF FILE *****************************/

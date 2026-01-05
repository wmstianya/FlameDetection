/**
  ******************************************************************************
  * @file           : flameDetector.h
  * @brief          : Flame detection business logic
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  * @attention
  * Implements state machine for flame detection with debouncing.
  * Separates business logic from hardware drivers.
  ******************************************************************************
  */

#ifndef __FLAME_DETECTOR_H
#define __FLAME_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "../config/systemConfig.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Flame detection states
  */
typedef enum {
    FLAME_STATE_INIT,               /* Initial state after reset */
    FLAME_STATE_STARTUP_DELAY,      /* Ignoring flame during startup */
    FLAME_STATE_DETECTED,           /* Flame is present */
    FLAME_STATE_LOST_DEBOUNCE,      /* Flame lost, debouncing */
    FLAME_STATE_LOST_CONFIRMED      /* Flame loss confirmed */
} FlameState;

/**
  * @brief  Flame detector context structure
  */
typedef struct {
    FlameState state;               /* Current state */
    uint32_t stateEntryTime;        /* Time when entered current state */
    uint32_t voltageMv;             /* Latest voltage reading (mV) */
    uint16_t thresholdMv;           /* Detection threshold (mV) */
    uint16_t startupDelayMs;        /* Startup delay time */
    uint16_t offDelayMs;            /* Flame off debounce time */
    uint32_t flameOffCount;         /* Flame off counter (ms) */
    uint32_t firstStartCount;       /* Startup counter (ms) */
} FlameDetector;

/* Exported functions --------------------------------------------------------*/

/* Initialization */
void flameDetectorInit(FlameDetector* detector, const SystemConfig* config);
void flameDetectorReset(FlameDetector* detector);

/* State machine update */
void flameDetectorUpdate(FlameDetector* detector, uint32_t voltageMv, uint32_t nowMs);

/* State queries */
FlameState flameDetectorGetState(const FlameDetector* detector);
uint8_t flameDetectorIsFlamePresent(const FlameDetector* detector);
uint8_t flameDetectorIsStartupComplete(const FlameDetector* detector);

/* Control outputs */
void flameDetectorApplyOutputs(const FlameDetector* detector);
void flameDetectorSetLed(uint8_t state);
void flameDetectorSetRelay(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* __FLAME_DETECTOR_H */

/************************ END OF FILE *****************************/


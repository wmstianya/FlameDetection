/**
  ******************************************************************************
  * @file           : flameDetector.c
  * @brief          : Flame detection state machine implementation
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  * @attention
  * All functions comply with 20-line maximum rule.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "flameDetector.h"

/* ============================================================================
   Initialization Functions
   ============================================================================ */

/**
  * @brief  Initialize flame detector
  * @param  detector: pointer to detector structure
  * @param  config: pointer to system configuration
  * @retval None
  */
void flameDetectorInit(FlameDetector* detector, const SystemConfig* config)
{
    detector->state = FLAME_STATE_INIT;
    detector->stateEntryTime = 0;
    detector->voltageMv = 0;
    detector->thresholdMv = config->flameThresholdMv;
    detector->startupDelayMs = config->startupDelayMs;
    detector->offDelayMs = config->flameOffDelayMs;
    detector->flameOffCount = 0;
    detector->firstStartCount = 0;
}

/**
  * @brief  Reset flame detector to initial state
  * @param  detector: pointer to detector structure
  * @retval None
  */
void flameDetectorReset(FlameDetector* detector)
{
    detector->state = FLAME_STATE_INIT;
    detector->stateEntryTime = HAL_GetTick();
    detector->flameOffCount = 0;
    detector->firstStartCount = 0;
}

/* ============================================================================
   State Transition Functions
   ============================================================================ */

/**
  * @brief  Transition to new state
  * @param  detector: pointer to detector structure
  * @param  newState: new state to enter
  * @param  nowMs: current timestamp
  * @retval None
  */
static void transitionToState(FlameDetector* detector, FlameState newState, uint32_t nowMs)
{
    detector->state = newState;
    detector->stateEntryTime = nowMs;
}

/**
  * @brief  Check if voltage indicates flame
  * @param  detector: pointer to detector structure
  * @retval 1 if flame detected, 0 otherwise
  */
static uint8_t isFlameVoltage(const FlameDetector* detector)
{
    return (detector->voltageMv < detector->thresholdMv) ? 1 : 0;
}

/**
  * @brief  Get time in current state
  * @param  detector: pointer to detector structure
  * @param  nowMs: current timestamp
  * @retval Time in ms
  */
static uint32_t getTimeInState(const FlameDetector* detector, uint32_t nowMs)
{
    return nowMs - detector->stateEntryTime;
}

/* ============================================================================
   State Handlers
   ============================================================================ */

/**
  * @brief  Handle INIT state
  * @param  detector: pointer to detector structure
  * @param  nowMs: current timestamp
  * @retval None
  */
static void handleInitState(FlameDetector* detector, uint32_t nowMs)
{
    transitionToState(detector, FLAME_STATE_STARTUP_DELAY, nowMs);
}

/**
  * @brief  Handle STARTUP_DELAY state
  * @param  detector: pointer to detector structure
  * @param  nowMs: current timestamp
  * @retval None
  */
static void handleStartupDelayState(FlameDetector* detector, uint32_t nowMs)
{
    if (detector->firstStartCount >= detector->startupDelayMs) {
        if (isFlameVoltage(detector)) {
            transitionToState(detector, FLAME_STATE_DETECTED, nowMs);
        } else {
            transitionToState(detector, FLAME_STATE_LOST_CONFIRMED, nowMs);
        }
    }
}

/**
  * @brief  Handle DETECTED state
  * @param  detector: pointer to detector structure
  * @param  nowMs: current timestamp
  * @retval None
  */
static void handleDetectedState(FlameDetector* detector, uint32_t nowMs)
{
    if (!isFlameVoltage(detector)) {
        detector->flameOffCount = 0;
        transitionToState(detector, FLAME_STATE_LOST_DEBOUNCE, nowMs);
    } else {
        detector->flameOffCount = 0;
    }
}

/**
  * @brief  Handle LOST_DEBOUNCE state
  * @param  detector: pointer to detector structure
  * @param  nowMs: current timestamp
  * @retval None
  */
static void handleLostDebounceState(FlameDetector* detector, uint32_t nowMs)
{
    if (isFlameVoltage(detector)) {
        transitionToState(detector, FLAME_STATE_DETECTED, nowMs);
    } else if (detector->flameOffCount > detector->offDelayMs) {
        transitionToState(detector, FLAME_STATE_LOST_CONFIRMED, nowMs);
    }
}

/**
  * @brief  Handle LOST_CONFIRMED state
  * @param  detector: pointer to detector structure
  * @param  nowMs: current timestamp
  * @retval None
  */
static void handleLostConfirmedState(FlameDetector* detector, uint32_t nowMs)
{
    if (isFlameVoltage(detector)) {
        transitionToState(detector, FLAME_STATE_DETECTED, nowMs);
    }
}

/* ============================================================================
   State Machine Update
   ============================================================================ */

/**
  * @brief  Update flame detector state machine
  * @param  detector: pointer to detector structure
  * @param  voltageMv: current voltage reading (mV)
  * @param  nowMs: current timestamp
  * @retval None
  */
void flameDetectorUpdate(FlameDetector* detector, uint32_t voltageMv, uint32_t nowMs)
{
    detector->voltageMv = voltageMv;
    
    switch (detector->state) {
        case FLAME_STATE_INIT:
            handleInitState(detector, nowMs);
            break;
            
        case FLAME_STATE_STARTUP_DELAY:
            handleStartupDelayState(detector, nowMs);
            break;
            
        case FLAME_STATE_DETECTED:
            handleDetectedState(detector, nowMs);
            break;
            
        case FLAME_STATE_LOST_DEBOUNCE:
            handleLostDebounceState(detector, nowMs);
            break;
            
        case FLAME_STATE_LOST_CONFIRMED:
            handleLostConfirmedState(detector, nowMs);
            break;
            
        default:
            detector->state = FLAME_STATE_INIT;
            break;
    }
}

/* ============================================================================
   State Query Functions
   ============================================================================ */

/**
  * @brief  Get current state
  * @param  detector: pointer to detector structure
  * @retval Current state
  */
FlameState flameDetectorGetState(const FlameDetector* detector)
{
    return detector->state;
}

/**
  * @brief  Check if flame is present
  * @param  detector: pointer to detector structure
  * @retval 1 if flame present, 0 otherwise
  */
uint8_t flameDetectorIsFlamePresent(const FlameDetector* detector)
{
    return (detector->state == FLAME_STATE_DETECTED) ? 1 : 0;
}

/**
  * @brief  Check if startup delay is complete
  * @param  detector: pointer to detector structure
  * @retval 1 if complete, 0 otherwise
  */
uint8_t flameDetectorIsStartupComplete(const FlameDetector* detector)
{
    return (detector->firstStartCount >= detector->startupDelayMs) ? 1 : 0;
}

/* ============================================================================
   Output Control Functions
   ============================================================================ */

/**
  * @brief  Set LED state
  * @param  state: 1 = ON, 0 = OFF
  * @retval None
  */
void flameDetectorSetLed(uint8_t state)
{
    if (state) {
        HAL_GPIO_WritePin(FLAME_LED_PORT, FLAME_LED_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(FLAME_LED_PORT, FLAME_LED_PIN, GPIO_PIN_SET);
    }
}

/**
  * @brief  Set relay state
  * @param  state: 1 = ON, 0 = OFF
  * @retval None
  */
void flameDetectorSetRelay(uint8_t state)
{
    if (state) {
        HAL_GPIO_WritePin(FLAME_RELAY_PORT, FLAME_RELAY_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(FLAME_RELAY_PORT, FLAME_RELAY_PIN, GPIO_PIN_RESET);
    }
}

/**
  * @brief  Apply outputs based on current state
  * @param  detector: pointer to detector structure
  * @retval None
  */
void flameDetectorApplyOutputs(const FlameDetector* detector)
{
    if (detector->state == FLAME_STATE_DETECTED) {
        if (detector->firstStartCount >= detector->startupDelayMs) {
            flameDetectorSetLed(1);
            flameDetectorSetRelay(1);
        }
    } else if (detector->state == FLAME_STATE_LOST_CONFIRMED) {
        flameDetectorSetLed(0);
        flameDetectorSetRelay(0);
    }
}

/************************ END OF FILE *****************************/


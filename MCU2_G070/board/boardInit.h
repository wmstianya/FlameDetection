/**
 * @file    boardInit.h
 * @brief   Board bring-up: clock, GPIO, external-WDT feed and indicator LEDs.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  Added API documentation.
 */
#ifndef BOARD_INIT_H
#define BOARD_INIT_H

/**
 * @brief  Initialise HAL, 64 MHz clock tree and indicator/WDI GPIO.
 * @return None.
 */
void boardInit(void);

/**
 * @brief  1 kHz SysTick service: advances the tick and drives WDI/LED timers.
 * @return None.
 */
void boardSysTickInc(void);

/**
 * @brief  Main-loop WDI hook (see A2 watchdog-liveness plan).
 * @return None.
 */
void wdiFeedToggle(void);

/**
 * @brief  Start a short communication-LED pulse (auto-cleared by SysTick).
 * @return None.
 */
void ledComPulse(void);

/**
 * @brief  Toggle the run/heartbeat LED at its fixed period.
 * @return None.
 */
void runLedHeartbeat(void);

#endif /* BOARD_INIT_H */

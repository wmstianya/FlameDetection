/**
 * @file    boardInit.h
 * @brief   Board bring-up: clock, GPIO, WDI and LEDs
 */
#ifndef BOARD_INIT_H
#define BOARD_INIT_H

void boardInit(void);
void boardSysTickInc(void);
void wdiFeedToggle(void);
void ledComPulse(void);
void runLedHeartbeat(void);

#endif /* BOARD_INIT_H */

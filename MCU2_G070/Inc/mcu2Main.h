/**
 * @file    mcu2Main.h
 * @brief   MCU2 application common header.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0
 */
#ifndef MCU2_MAIN_H
#define MCU2_MAIN_H

#include "stm32g0xx_hal.h"

void Error_Handler(void);
uint32_t boardGetMsTick(void);

#endif /* MCU2_MAIN_H */

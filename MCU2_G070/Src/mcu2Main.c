/**
 * @file    mcu2Main.c
 * @brief   Common error handler for MCU2.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0
 */
#include "mcu2Main.h"

/**
 * @brief  Fatal-error trap. Disabling IRQs stops the SysTick-driven WDI feed,
 *         so the external watchdog resets the board (intended recovery path).
 * @return Does not return.
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

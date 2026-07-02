/**
 * @file    mcu2Main.c
 * @brief   Common error handler for MCU2
 */
#include "mcu2Main.h"

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

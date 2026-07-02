/**
 * @file    stm32g0xx_it.c
 * @brief   MCU2 interrupt service routines.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0
 */
#include "stm32g0xx_it.h"
#include "stm32g0xx_hal.h"
#include "../board/boardInit.h"
#include "../config/mcu2Pins.h"
#include "../spi/spiSlave.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    boardSysTickInc();
}

void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(spiSlaveGetHandle());
}

void EXTI4_15_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(MCU2_SPI1_CS_PIN);
}

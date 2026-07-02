/**
 * @file    stm32g0xx_hal_msp.c
 * @brief   MCU2 HAL MSP — SPI1
 */
#include "stm32g0xx_hal.h"

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_DISABLE();
    }
}

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

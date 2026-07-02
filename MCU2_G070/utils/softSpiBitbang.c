/**
 * @file    softSpiBitbang.c
 * @brief   ADS1220 soft SPI Mode 1 (CPOL=0, CPHA=1); matches ADS1220.h on F103.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  Documented the busy-wait cycle model.
 */
#include "softSpiBitbang.h"
#include "../config/mcu2Pins.h"
#include "stm32g0xx_hal.h"

#define SOFT_SPI_EDGE_DELAY_US   20U
#define CYCLES_PER_US            (SystemCoreClock / 1000000U)
#define BUSY_LOOP_CYCLE_COST     3U   /* approx. cycles per decrement+NOP iter */

/**
 * @brief  Coarse busy-wait used to shape the bit-bang half-period.
 * @param  us Approximate delay in microseconds.
 * @return None.
 * @note   Bounded loop (no watchdog concern); accuracy is not critical because
 *         the ADS1220 tolerates a wide clock range.
 */
static void softSpiDelayUs(uint32_t us)
{
    uint32_t count = (CYCLES_PER_US * us) / BUSY_LOOP_CYCLE_COST;
    while (count > 0U)
    {
        count--;
        __NOP();
    }
}

void softSpiBitbangInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = MCU2_ADS_CS_PIN | MCU2_ADS_SCK_PIN | MCU2_ADS_MOSI_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = MCU2_ADS_DRDY_PIN | MCU2_ADS_MISO_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(MCU2_ADS_CS_PORT, MCU2_ADS_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MCU2_ADS_MOSI_PORT, MCU2_ADS_MOSI_PIN, GPIO_PIN_RESET);
}

void softSpiBitbangCsSet(uint8_t enable)
{
    HAL_GPIO_WritePin(MCU2_ADS_CS_PORT, MCU2_ADS_CS_PIN,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void softSpiBitbangSendByte(uint8_t data)
{
    uint8_t bit;
    HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_RESET);
    for (bit = 0U; bit < 8U; bit++)
    {
        HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_SET);
        softSpiDelayUs(SOFT_SPI_EDGE_DELAY_US);
        if ((data & 0x80U) != 0U)
            HAL_GPIO_WritePin(MCU2_ADS_MOSI_PORT, MCU2_ADS_MOSI_PIN, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(MCU2_ADS_MOSI_PORT, MCU2_ADS_MOSI_PIN, GPIO_PIN_RESET);
        softSpiDelayUs(SOFT_SPI_EDGE_DELAY_US);
        data <<= 1;
        HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_RESET);
        softSpiDelayUs(SOFT_SPI_EDGE_DELAY_US);
    }
}

uint8_t softSpiBitbangRecvByte(void)
{
    uint8_t bit;
    uint8_t data = 0U;
    for (bit = 0U; bit < 8U; bit++)
    {
        HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_SET);
        softSpiDelayUs(SOFT_SPI_EDGE_DELAY_US);
        data <<= 1;
        if (HAL_GPIO_ReadPin(MCU2_ADS_MISO_PORT, MCU2_ADS_MISO_PIN) == GPIO_PIN_SET)
            data |= 0x01U;
        softSpiDelayUs(SOFT_SPI_EDGE_DELAY_US);
        HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_RESET);
        softSpiDelayUs(SOFT_SPI_EDGE_DELAY_US);
    }
    HAL_GPIO_WritePin(MCU2_ADS_SCK_PORT, MCU2_ADS_SCK_PIN, GPIO_PIN_RESET);
    return data;
}

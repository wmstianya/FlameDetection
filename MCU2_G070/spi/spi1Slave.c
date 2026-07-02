/**
 * @file    spi1Slave.c
 * @brief   SPI1 slave Mode0; NSS falling edge starts 5-byte IT exchange
 */
#include "spi1Slave.h"
#include "../config/mcu2Pins.h"
#include "../Inc/mcu2Main.h"

static SPI_HandleTypeDef hspi1;
static HostFrame gTxFrame;
static HostFrame gRxFrame;
static HostCommand gLastCmd;
static volatile uint8 gFrameDone = 0U;
static volatile uint8 gTransferActive = 0U;

SPI_HandleTypeDef *spi1SlaveGetHandle(void)
{
    return &hspi1;
}

static void spi1SlaveGpioInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF0_SPI1;
    gpio.Pin = MCU2_SPI1_CLK_PIN | MCU2_SPI1_MISO_PIN | MCU2_SPI1_MOSI_PIN;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void spi1SlavePeriphInit(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_SLAVE;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7U;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
        Error_Handler();
    HAL_NVIC_SetPriority(SPI1_IRQn, 0U, 0U);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

static void spi1SlaveNssExtiInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = MCU2_SPI1_CS_PIN;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MCU2_SPI1_CS_PORT, &gpio);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

static void spi1SlaveStartTransfer(void)
{
    if (gTransferActive != 0U)
        return;
    if (hspi1.State != HAL_SPI_STATE_READY)
        return;
    gTransferActive = 1U;
    if (HAL_SPI_TransmitReceive_IT(&hspi1, gTxFrame.bytes, gRxFrame.bytes,
                                    HOST_FRAME_LEN) != HAL_OK)
        gTransferActive = 0U;
}

void spi1SlaveInit(void)
{
    hostBuildResponse(&gTxFrame, 300U, HOST_STAT_OK);
    gFrameDone = 0U;
    gTransferActive = 0U;
    spi1SlaveGpioInit();
    spi1SlavePeriphInit();
    spi1SlaveNssExtiInit();
}

void spi1SlaveSetResponse(const HostFrame *tx)
{
    if (tx == NULL)
        return;
    gTxFrame = *tx;
}

uint8 spi1SlaveFrameComplete(void)
{
    if (gFrameDone == 0U)
        return 0U;
    gFrameDone = 0U;
    return 1U;
}

void spi1SlavePoll(void)
{
}

void spi1SlaveOnNssFalling(void)
{
    spi1SlaveStartTransfer();
}

void spi1SlaveOnTransferComplete(void)
{
    HostFrame rx;
    uint8 i;

    for (i = 0U; i < HOST_FRAME_LEN; i++)
        rx.bytes[i] = gRxFrame.bytes[i];
    (void)hostParseCommand(&rx, &gLastCmd);
    gTransferActive = 0U;
    gFrameDone = 1U;
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == MCU2_SPI1_CS_PIN)
        spi1SlaveOnNssFalling();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spi1SlaveOnTransferComplete();
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        gTransferActive = 0U;
}

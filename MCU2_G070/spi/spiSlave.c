/**
 * @file    spiSlave.c
 * @brief   SPI1 slave, Mode 0; NSS falling edge starts a 5-byte IT exchange.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  Renamed spi1Slave->spiSlave; removed unused poll hook.
 */
#include "spiSlave.h"
#include "../config/mcu2Pins.h"
#include "../Inc/mcu2Main.h"

static SPI_HandleTypeDef spiSlaveHandle;
static HostFrame gTxFrame;
static HostFrame gRxFrame;
static HostCommand gLastCmd;
static volatile uint8 gFrameDone = 0U;
static volatile uint8 gTransferActive = 0U;

/* Provisional temperature staged before the first ADS1220 reading. */
#define SPI_SLAVE_SEED_TEMP_C   300U

SPI_HandleTypeDef *spiSlaveGetHandle(void)
{
    return &spiSlaveHandle;
}

/**
 * @brief  Configure SCK/MISO/MOSI (PA5-PA7) as SPI1 alternate function.
 * @return None.
 */
static void spiSlaveGpioInit(void)
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

/**
 * @brief  Initialise the SPI1 peripheral as an 8-bit Mode 0 slave.
 * @return None.
 */
static void spiSlavePeriphInit(void)
{
    spiSlaveHandle.Instance = SPI1;
    spiSlaveHandle.Init.Mode = SPI_MODE_SLAVE;
    spiSlaveHandle.Init.Direction = SPI_DIRECTION_2LINES;
    spiSlaveHandle.Init.DataSize = SPI_DATASIZE_8BIT;
    spiSlaveHandle.Init.CLKPolarity = SPI_POLARITY_LOW;
    spiSlaveHandle.Init.CLKPhase = SPI_PHASE_1EDGE;
    spiSlaveHandle.Init.NSS = SPI_NSS_SOFT;
    spiSlaveHandle.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spiSlaveHandle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    spiSlaveHandle.Init.CRCPolynomial = 7U;
    if (HAL_SPI_Init(&spiSlaveHandle) != HAL_OK)
        Error_Handler();
    HAL_NVIC_SetPriority(SPI1_IRQn, 0U, 0U);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

/**
 * @brief  Arm the NSS (PA4) falling-edge EXTI that starts each transaction.
 * @return None.
 */
static void spiSlaveNssExtiInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = MCU2_SPI1_CS_PIN;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MCU2_SPI1_CS_PORT, &gpio);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/**
 * @brief  Kick off an interrupt-driven 5-byte full-duplex exchange.
 * @return None.
 */
static void spiSlaveStartTransfer(void)
{
    if (gTransferActive != 0U)
        return;
    if (spiSlaveHandle.State != HAL_SPI_STATE_READY)
        return;
    gTransferActive = 1U;
    if (HAL_SPI_TransmitReceive_IT(&spiSlaveHandle, gTxFrame.bytes, gRxFrame.bytes,
                                    HOST_FRAME_LEN) != HAL_OK)
        gTransferActive = 0U;
}

void spiSlaveInit(void)
{
    hostBuildResponse(&gTxFrame, SPI_SLAVE_SEED_TEMP_C, HOST_STAT_OK);
    gFrameDone = 0U;
    gTransferActive = 0U;
    spiSlaveGpioInit();
    spiSlavePeriphInit();
    spiSlaveNssExtiInit();
}

void spiSlaveSetResponse(const HostFrame *tx)
{
    if (tx == NULL)
        return;
    gTxFrame = *tx;
}

uint8 spiSlaveFrameComplete(void)
{
    if (gFrameDone == 0U)
        return 0U;
    gFrameDone = 0U;
    return 1U;
}

/**
 * @brief  NSS falling-edge hook: begin the next transaction.
 * @return None.
 */
static void spiSlaveOnNssFalling(void)
{
    spiSlaveStartTransfer();
}

/**
 * @brief  Transfer-complete hook: parse the received command frame.
 * @return None.
 */
static void spiSlaveOnTransferComplete(void)
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
        spiSlaveOnNssFalling();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spiSlaveOnTransferComplete();
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        gTransferActive = 0U;
}

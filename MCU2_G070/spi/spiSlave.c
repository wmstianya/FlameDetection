/**
 * @file    spiSlave.c
 * @brief   SPI1 slave, Mode 0; NSS-driven 5-byte exchange with frame self-heal.
 * @details Keeps a 5-byte HAL_SPI_TransmitReceive_IT continuously armed so the
 *          first response byte (constant 0x68 head) is preloaded before the
 *          master ever asserts CS. On the CS(NSS) RISING edge the frame is
 *          re-aligned: a transfer that did not complete is aborted and re-armed
 *          from byte 0. This is what lets the D380 master self-heal a
 *          de-synchronised link (see spiFraming.h).
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.2.0  NSS both-edge EXTI, rising-edge resync, first-byte preload.
 */
#include "spiSlave.h"
#include "spiFraming.h"
#include "../config/mcu2Pins.h"
#include "../Inc/mcu2Main.h"

static SPI_HandleTypeDef spiSlaveHandle;
static HostFrame gTxFrame;
static HostFrame gRxFrame;
static HostCommand gLastCmd;
static SpiFraming gFraming;

/* Provisional temperature staged before the first ADS1220 reading. */
#define SPI_SLAVE_SEED_TEMP_C   300U

SPI_HandleTypeDef *spiSlaveGetHandle(void)
{
    return &spiSlaveHandle;
}

/**
 * @brief  Enter a short interrupt-masked critical section.
 * @return Saved PRIMASK to restore with spiSlaveExitCritical().
 */
static uint32_t spiSlaveEnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/**
 * @brief  Restore the interrupt state saved by spiSlaveEnterCritical().
 * @param  primask Value previously returned by spiSlaveEnterCritical().
 * @return None.
 */
static void spiSlaveExitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
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
 * @brief  Arm the NSS (PA4) EXTI on BOTH edges: falling starts a transaction,
 *         rising closes the window and drives frame re-alignment.
 * @return None.
 */
static void spiSlaveNssExtiInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = MCU2_SPI1_CS_PIN;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MCU2_SPI1_CS_PORT, &gpio);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/**
 * @brief  Arm a fresh 5-byte transfer (assumes the peripheral is READY).
 * @details The first TXE loads gTxFrame.bytes[0] (constant 0x68 head) into the
 *          shift register immediately, so byte 0 is preloaded ahead of CS.
 * @return None.
 */
static void spiSlaveArm(void)
{
    if (HAL_SPI_TransmitReceive_IT(&spiSlaveHandle, gTxFrame.bytes, gRxFrame.bytes,
                                   HOST_FRAME_LEN) != HAL_OK)
        spiFramingOnError(&gFraming);   /* let the service abort + re-arm */
}

void spiSlaveInit(void)
{
    hostBuildResponse(&gTxFrame, SPI_SLAVE_SEED_TEMP_C, HOST_STAT_OK);
    spiFramingInit(&gFraming);
    spiSlaveGpioInit();
    spiSlavePeriphInit();
    spiSlaveNssExtiInit();
    spiSlaveArm();                       /* preload byte 0 before first CS */
}

void spiSlaveService(void)
{
    uint8 doResync;
    uint32_t primask = spiSlaveEnterCritical();
    doResync = spiFramingConsumeResync(&gFraming);
    spiSlaveExitCritical(primask);

    if (doResync != 0U)
    {
        /* Flush the stalled/errored transfer back to READY, then re-align from
         * byte 0 with byte 0 preloaded. Runs in the idle gap between the 1 Hz
         * master polls, so no clock is active during the abort. */
        (void)HAL_SPI_Abort(&spiSlaveHandle);
        spiSlaveArm();
    }
}

void spiSlaveSetResponse(const HostFrame *tx)
{
    uint32_t primask;
    if (tx == NULL)
        return;
    primask = spiSlaveEnterCritical();
    if (spiFramingAllowResponseUpdate(&gFraming) != 0U)
        gTxFrame = *tx;                  /* atomic vs. a mid-frame CS assert */
    spiSlaveExitCritical(primask);
}

uint8 spiSlaveFrameComplete(void)
{
    uint8 ready;
    uint32_t primask = spiSlaveEnterCritical();
    ready = spiFramingConsumeFrameReady(&gFraming);
    spiSlaveExitCritical(primask);
    return ready;
}

/**
 * @brief  Transfer-complete hook: parse the frame and re-arm for the next one.
 * @return None.
 */
static void spiSlaveOnTransferComplete(void)
{
    HostFrame rx = gRxFrame;
    (void)hostParseCommand(&rx, &gLastCmd);
    spiFramingOnComplete(&gFraming);
    spiSlaveArm();                       /* continuous re-arm; state is READY */
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == MCU2_SPI1_CS_PIN)
        spiFramingOnCsAssert(&gFraming);
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == MCU2_SPI1_CS_PIN)
        (void)spiFramingOnCsDeassert(&gFraming);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spiSlaveOnTransferComplete();
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spiFramingOnError(&gFraming);
}

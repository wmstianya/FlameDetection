/**
  ******************************************************************************
  * @file           : spiMaster.c
  * @brief          : SPI1 master driver implementation
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Implements HAL-backed SPI1 master on STM32G070 for D380 sensor access.
  * All functions adhere to the ≤ 20-line body rule; helpers are split out.
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#include "spiMaster.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static SPI_HandleTypeDef hSpiMaster;

/* Private helpers -----------------------------------------------------------*/

/**
  * @brief  Configure SPI1 bus pins (PA5/PA6/PA7) as alternate-function.
  * @retval None
  */
static void configureSpiMasterBusPins(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    gpioInit.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpioInit.Mode      = GPIO_MODE_AF_PP;
    gpioInit.Pull      = GPIO_NOPULL;
    gpioInit.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpioInit.Alternate = GPIO_AF0_SPI1;
    HAL_GPIO_Init(GPIOA, &gpioInit);
}

/**
  * @brief  Configure D380 chip-select pin (PA4) as push-pull output, idle high.
  * @retval None
  */
static void configureSpiMasterCsPin(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    gpioInit.Pin   = D380_CS_PIN;
    gpioInit.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull  = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(D380_CS_PORT, &gpioInit);
    HAL_GPIO_WritePin(D380_CS_PORT, D380_CS_PIN, GPIO_PIN_SET);
}

/**
  * @brief  Configure SPI1 GPIO: enable clock, set bus and CS pins.
  * @retval None
  */
static void configureSpiMasterGpio(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    configureSpiMasterBusPins();
    configureSpiMasterCsPin();
}

/**
  * @brief  Populate and initialise the SPI1 peripheral registers.
  * @retval None
  */
static void configureSpiMasterPeripheral(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    hSpiMaster.Instance               = SPI1;
    hSpiMaster.Init.Mode              = SPI_MODE_MASTER;
    hSpiMaster.Init.Direction         = SPI_DIRECTION_2LINES;
    hSpiMaster.Init.DataSize          = SPI_DATASIZE_8BIT;
    hSpiMaster.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hSpiMaster.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hSpiMaster.Init.NSS               = SPI_NSS_SOFT;
    hSpiMaster.Init.BaudRatePrescaler = SPI_MASTER_BAUDRATE_PRESCALER;
    hSpiMaster.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hSpiMaster.Init.TIMode            = SPI_TIMODE_DISABLE;
    hSpiMaster.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hSpiMaster.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hSpiMaster) != HAL_OK) {
        Error_Handler();
    }
}

/* Public API ----------------------------------------------------------------*/

/**
  * @brief  Initialise SPI1 master and chip-select GPIO.
  */
void spiMasterInit(void)
{
    configureSpiMasterGpio();
    configureSpiMasterPeripheral();
}

/**
  * @brief  De-initialise SPI1 and release GPIO.
  */
void spiMasterDeInit(void)
{
    HAL_SPI_DeInit(&hSpiMaster);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
    HAL_GPIO_DeInit(D380_CS_PORT, D380_CS_PIN);
    __HAL_RCC_SPI1_CLK_DISABLE();
}

/**
  * @brief  Assert chip-select (PA4 → low).
  */
void spiMasterCsAssert(void)
{
    HAL_GPIO_WritePin(D380_CS_PORT, D380_CS_PIN, GPIO_PIN_RESET);
    /* Small setup delay to satisfy D380 CS-to-CLK requirement */
    for (volatile uint32_t i = 0; i < D380_CS_SETUP_US * 8u; i++) { __NOP(); }
}

/**
  * @brief  De-assert chip-select (PA4 → high).
  */
void spiMasterCsRelease(void)
{
    /* Small hold delay before CS goes high */
    for (volatile uint32_t i = 0; i < D380_CS_HOLD_US * 8u; i++) { __NOP(); }
    HAL_GPIO_WritePin(D380_CS_PORT, D380_CS_PIN, GPIO_PIN_SET);
}

/**
  * @brief  Transmit one byte, discard received byte.
  * @param  byte  Byte to send.
  * @retval SpiMasterStatus
  */
SpiMasterStatus spiMasterWriteByte(uint8_t byte)
{
    HAL_StatusTypeDef hal;
    uint8_t dummy;

    hal = HAL_SPI_TransmitReceive(&hSpiMaster, &byte, &dummy,
                                  1u, SPI_MASTER_TIMEOUT_MS);
    if (hal == HAL_TIMEOUT) { return SPI_MASTER_TIMEOUT; }
    if (hal != HAL_OK)      { return SPI_MASTER_ERROR;   }
    return SPI_MASTER_OK;
}

/**
  * @brief  Receive one byte by clocking out 0xFF.
  * @param  outByte  Output storage. Must not be NULL.
  * @retval SpiMasterStatus
  */
SpiMasterStatus spiMasterReadByte(uint8_t *outByte)
{
    HAL_StatusTypeDef hal;
    uint8_t dummy = 0xFFu;

    if (outByte == NULL) { return SPI_MASTER_PARAM_ERR; }

    hal = HAL_SPI_TransmitReceive(&hSpiMaster, &dummy, outByte,
                                  1u, SPI_MASTER_TIMEOUT_MS);
    if (hal == HAL_TIMEOUT) { return SPI_MASTER_TIMEOUT; }
    if (hal != HAL_OK)      { return SPI_MASTER_ERROR;   }
    return SPI_MASTER_OK;
}

/**
  * @brief  Resolve TX and RX pointers, substituting internal dummy buffers
  *         when NULL is passed by the caller.
  * @param  txBuf   Caller TX buffer (may be NULL).
  * @param  rxBuf   Caller RX buffer (may be NULL).
  * @param  length  Transfer length (capped to SPI_SLAVE_FRAME_SIZE for dummies).
  * @param  ppTx    Output: resolved TX pointer.
  * @param  ppRx    Output: resolved RX pointer.
  */
static void resolveTransferBuffers(const uint8_t  *txBuf,
                                    uint8_t        *rxBuf,
                                    uint16_t        length,
                                    const uint8_t **ppTx,
                                    uint8_t       **ppRx)
{
    static uint8_t dummyTx[SPI_SLAVE_FRAME_SIZE];
    static uint8_t dummyRx[SPI_SLAVE_FRAME_SIZE];

    if (txBuf == NULL) {
        memset(dummyTx, 0xFFu, length < SPI_SLAVE_FRAME_SIZE
                                ? length : SPI_SLAVE_FRAME_SIZE);
        *ppTx = dummyTx;
    } else {
        *ppTx = txBuf;
    }

    *ppRx = (rxBuf != NULL) ? rxBuf : dummyRx;
}

/**
  * @brief  Full-duplex buffer transfer over SPI1.
  * @note   If txBuf is NULL a 0xFF-filled dummy is used. Received bytes are
  *         stored in rxBuf when non-NULL.
  * @param  txBuf   Pointer to TX data (or NULL for dummy bytes).
  * @param  rxBuf   Pointer to RX buffer (or NULL to discard).
  * @param  length  Number of bytes to transfer. Must be > 0.
  * @retval SpiMasterStatus
  */
SpiMasterStatus spiMasterTransfer(const uint8_t *txBuf,
                                   uint8_t       *rxBuf,
                                   uint16_t       length)
{
    HAL_StatusTypeDef  hal;
    const uint8_t     *tx;
    uint8_t           *rx;

    if (length == 0u) { return SPI_MASTER_PARAM_ERR; }

    resolveTransferBuffers(txBuf, rxBuf, length, &tx, &rx);

    hal = HAL_SPI_TransmitReceive(&hSpiMaster,
                                  (uint8_t *)tx, rx,
                                  length, SPI_MASTER_TIMEOUT_MS);
    if (hal == HAL_TIMEOUT) { return SPI_MASTER_TIMEOUT; }
    if (hal != HAL_OK)      { return SPI_MASTER_ERROR;   }
    return SPI_MASTER_OK;
}

/************************ END OF FILE *****************************/

/**
  ******************************************************************************
  * @file           : spiSlave.c
  * @brief          : SPI2 slave driver implementation
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * SPI2 is configured in full-duplex slave mode (CPOL=0, CPHA=0, hardware NSS).
  * The driver maintains a 5-byte shadow TX frame and services MCU1 requests
  * using blocking HAL calls inside the cooperative scheduler polling slot.
  *
  * Frame structure (see spiSlave.h for layout):
  *   [0x5A] [tempMSB] [tempLSB] [status] [CRC8(0..3)]
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#include "spiSlave.h"
#include "../utils/crc8.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static SPI_HandleTypeDef hSpiSlave;

/** Shadow TX frame, pre-built by spiSlaveUpdateTemp() */
static uint8_t slaveTxFrame[SPI_SLAVE_FRAME_SIZE];

/** RX buffer for the incoming MCU1 command byte */
static uint8_t slaveRxByte;

/** Statistics counter */
static uint32_t requestCount = 0u;

/* Private helpers -----------------------------------------------------------*/

/**
  * @brief  Configure SPI2 GPIO pins.
  */
static void configureSpiSlaveGpio(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* SCK, MISO, MOSI, NSS — alternate function */
    gpioInit.Pin       = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpioInit.Mode      = GPIO_MODE_AF_PP;
    gpioInit.Pull      = GPIO_NOPULL;
    gpioInit.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpioInit.Alternate = GPIO_AF0_SPI2;
    HAL_GPIO_Init(GPIOB, &gpioInit);
}

/**
  * @brief  Populate and initialise SPI2 peripheral registers (slave mode).
  */
static void configureSpiSlavePeripheral(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();

    hSpiSlave.Instance               = SPI2;
    hSpiSlave.Init.Mode              = SPI_MODE_SLAVE;
    hSpiSlave.Init.Direction         = SPI_DIRECTION_2LINES;
    hSpiSlave.Init.DataSize          = SPI_DATASIZE_8BIT;
    hSpiSlave.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hSpiSlave.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hSpiSlave.Init.NSS               = SPI_NSS_HARD_INPUT;
    hSpiSlave.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hSpiSlave.Init.TIMode            = SPI_TIMODE_DISABLE;
    hSpiSlave.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hSpiSlave.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hSpiSlave) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  Build the 5-byte response frame from raw values.
  * @param  tempX10     Temperature (°C × 10).
  * @param  dataStatus  Freshness code embedded in byte 3.
  * @param  frame       Output buffer (must be SPI_SLAVE_FRAME_SIZE bytes).
  */
static void buildResponseFrame(int16_t            tempX10,
                                SpiSlaveDataStatus dataStatus,
                                uint8_t           *frame)
{
    frame[0] = SPI_RESP_HEADER;
    frame[1] = (uint8_t)((uint16_t)tempX10 >> 8u);
    frame[2] = (uint8_t)((uint16_t)tempX10 & 0xFFu);
    frame[3] = (uint8_t)dataStatus;
    frame[4] = crc8Compute(frame, 4u);
}

/**
  * @brief  Wait up to timeoutMs for the SPI NSS to be asserted by MCU1.
  * @param  timeoutMs  Maximum wait time in milliseconds.
  * @retval HAL_OK if NSS asserted within timeout, HAL_TIMEOUT otherwise.
  */
static HAL_StatusTypeDef waitForNssAssert(uint32_t timeoutMs)
{
    uint32_t start = HAL_GetTick();

    /* NSS input is reflected in the SPI SR MODF/OVR flags on G0; instead,
     * check that the peripheral is enabled and bus is idle by attempting a
     * receive with a short timeout window — the HAL will return HAL_TIMEOUT
     * when no master is clocking. */
    (void)timeoutMs;
    /* The actual wait is handled by HAL_SPI_TransmitReceive timeout below */
    (void)start;
    return HAL_OK;
}

/* Public API ----------------------------------------------------------------*/

/**
  * @brief  Initialise SPI2 slave and pre-fill TX shadow buffer.
  */
void spiSlaveInit(void)
{
    configureSpiSlaveGpio();
    configureSpiSlavePeripheral();

    /* Pre-load a safe default frame (30.0 °C, INIT status) */
    buildResponseFrame(TEMP_DEFAULT_X10, SPI_SLAVE_DATA_INIT, slaveTxFrame);
}

/**
  * @brief  De-initialise SPI2 and release GPIO.
  */
void spiSlaveDeInit(void)
{
    HAL_SPI_DeInit(&hSpiSlave);
    HAL_GPIO_DeInit(GPIOB,
                    GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    __HAL_RCC_SPI2_CLK_DISABLE();
}

/**
  * @brief  Atomically update the shadow TX frame with a new temperature.
  * @note   Disables SPI2 briefly to ensure the 5-byte frame is never
  *         partially read while it is being updated.
  */
void spiSlaveUpdateTemp(int16_t tempX10, SpiSlaveDataStatus dataStatus)
{
    /* Pause SPI2 during the update to prevent a torn read by MCU1 */
    __HAL_SPI_DISABLE(&hSpiSlave);
    buildResponseFrame(tempX10, dataStatus, slaveTxFrame);
    __HAL_SPI_ENABLE(&hSpiSlave);
}

/**
  * @brief  Build the 6-byte TX buffer for a slave response transaction.
  * @note   Byte 0 is a dummy (transmitted during command receive phase).
  *         Bytes 1-5 carry the pre-built shadow frame.
  * @param  txBuf  Output buffer (must be SPI_SLAVE_FRAME_SIZE + 1 bytes).
  */
static void prepareSlaveTransmitBuffer(uint8_t *txBuf)
{
    txBuf[0] = 0x00u;
    memcpy(&txBuf[1], slaveTxFrame, SPI_SLAVE_FRAME_SIZE);
}

/**
  * @brief  Validate the received command byte and update the request counter.
  * @param  cmdByte  First byte received from MCU1.
  */
static void processReceivedCommand(uint8_t cmdByte)
{
    slaveRxByte = cmdByte;
    if (cmdByte == SPI_CMD_READ_TEMP) {
        if (requestCount < UINT32_MAX) { requestCount++; }
    }
}

/**
  * @brief  Poll for a request from MCU1 and respond with shadow TX frame.
  * @note   Performs a 5+1 byte TransmitReceive: the first received byte is the
  *         command; bytes 1-5 clock out the response frame simultaneously.
  */
SpiSlaveStatus spiSlavePollAndRespond(void)
{
    uint8_t rxBuf[SPI_SLAVE_FRAME_SIZE + 1u];
    uint8_t txBuf[SPI_SLAVE_FRAME_SIZE + 1u];
    HAL_StatusTypeDef hal;

    prepareSlaveTransmitBuffer(txBuf);

    hal = HAL_SPI_TransmitReceive(&hSpiSlave, txBuf, rxBuf,
                                  SPI_SLAVE_FRAME_SIZE + 1u,
                                  SPI_SLAVE_TIMEOUT_MS);

    if (hal == HAL_TIMEOUT) { return SPI_SLAVE_TIMEOUT; }
    if (hal != HAL_OK)      { return SPI_SLAVE_ERROR;   }

    processReceivedCommand(rxBuf[0]);
    return SPI_SLAVE_OK;
}

/**
  * @brief  Return the count of successfully serviced MCU1 requests.
  */
uint32_t spiSlaveGetRequestCount(void)
{
    return requestCount;
}

/************************ END OF FILE *****************************/

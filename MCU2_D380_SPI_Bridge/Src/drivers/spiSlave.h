/**
  ******************************************************************************
  * @file           : spiSlave.h
  * @brief          : SPI2 slave driver interface for MCU1 communication
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * MCU2 presents itself as an SPI slave on SPI2 so that MCU1 can request
  * temperature data at any time. The protocol is simple and deterministic:
  *
  *   MCU1 → MCU2  : [SPI_CMD_READ_TEMP  (1 byte)]
  *   MCU2 → MCU1  : [SPI_RESP_HEADER (1 byte)] [tempX10 big-endian int16 (2 bytes)]
  *                   [status byte (1 byte)] [CRC8 over bytes 0-3 (1 byte)]
  *
  * Frame layout (SPI_SLAVE_FRAME_SIZE = 5):
  *   Byte 0 : SPI_RESP_HEADER  (0x5A)
  *   Byte 1 : tempX10 MSB
  *   Byte 2 : tempX10 LSB
  *   Byte 3 : status code (SpiSlaveStatus enum value)
  *   Byte 4 : CRC8 of bytes 0-3
  *
  * Pin assignments:
  *   PB13 = SPI2_SCK  (AF0)
  *   PB14 = SPI2_MISO (AF0)
  *   PB15 = SPI2_MOSI (AF0)
  *   PB12 = SPI2_NSS  (AF0, hardware NSS)
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __SPI_SLAVE_H
#define __SPI_SLAVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "../config/bridgeConfig.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Status codes embedded in the slave response frame.
  */
typedef enum {
    SPI_SLAVE_DATA_VALID    = 0x00u, /**< Temperature data is fresh and valid */
    SPI_SLAVE_DATA_STALE    = 0x01u, /**< Data valid but older than 2 s */
    SPI_SLAVE_DATA_SENSOR_ERR = 0x02u, /**< D380 read error, last value used */
    SPI_SLAVE_DATA_INIT     = 0x03u  /**< Not yet obtained a first reading */
} SpiSlaveDataStatus;

/**
  * @brief  Return codes for SPI slave driver functions.
  */
typedef enum {
    SPI_SLAVE_OK = 0,
    SPI_SLAVE_TIMEOUT,
    SPI_SLAVE_ERROR,
    SPI_SLAVE_PARAM_ERR
} SpiSlaveStatus;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise SPI2 in slave mode and prepare the TX buffer.
  * @retval None
  */
void spiSlaveInit(void);

/**
  * @brief  De-initialise SPI2 and release GPIO resources.
  * @retval None
  */
void spiSlaveDeInit(void);

/**
  * @brief  Update the temperature value held in the SPI slave TX shadow buffer.
  * @note   Call this every time a new D380 reading is available. Thread-safe
  *         by disabling the SPI2 peripheral briefly during the atomic update.
  * @param  tempX10  Temperature in °C × 10.
  * @param  dataStatus  Data freshness indicator for the response frame.
  * @retval None
  */
void spiSlaveUpdateTemp(int16_t tempX10, SpiSlaveDataStatus dataStatus);

/**
  * @brief  Poll for an incoming MCU1 request and respond with the shadow buffer.
  * @note   Non-blocking: returns SPI_SLAVE_TIMEOUT immediately if no CS edge
  *         has occurred since the last call. Call from the main scheduler loop.
  * @retval SpiSlaveStatus
  */
SpiSlaveStatus spiSlavePollAndRespond(void);

/**
  * @brief  Retrieve the count of successfully serviced MCU1 requests.
  * @retval Request count (saturates at UINT32_MAX).
  */
uint32_t spiSlaveGetRequestCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_SLAVE_H */

/************************ END OF FILE *****************************/

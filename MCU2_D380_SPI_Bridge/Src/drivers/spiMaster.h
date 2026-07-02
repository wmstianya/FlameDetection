/**
  ******************************************************************************
  * @file           : spiMaster.h
  * @brief          : SPI1 master driver interface for D380 sensor communication
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Wraps HAL SPI1 in master mode (CPOL=0, CPHA=0, MSB-first, 8-bit frames).
  * Provides software chip-select management and byte-level transfer helpers
  * used by the D380 temperature sensor driver layer above.
  *
  * Pin assignments (see bridgeConfig.h for GPIO constants):
  *   PA5  SPI1_SCK   (AF0)
  *   PA6  SPI1_MISO  (AF0)
  *   PA7  SPI1_MOSI  (AF0)
  *   PA4  D380_CS    (GPIO output, active-low, software-controlled)
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __SPI_MASTER_H
#define __SPI_MASTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "../config/bridgeConfig.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Return codes for SPI master operations.
  */
typedef enum {
    SPI_MASTER_OK = 0,      /**< Operation completed successfully */
    SPI_MASTER_TIMEOUT,     /**< HAL transfer timed out */
    SPI_MASTER_ERROR,       /**< HAL reported a bus error */
    SPI_MASTER_PARAM_ERR    /**< NULL pointer or invalid parameter */
} SpiMasterStatus;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise SPI1 peripheral and D380 chip-select GPIO.
  * @retval None
  */
void spiMasterInit(void);

/**
  * @brief  De-initialise SPI1 peripheral and release GPIO resources.
  * @retval None
  */
void spiMasterDeInit(void);

/**
  * @brief  Assert the D380 chip-select line (drive PA4 low).
  * @retval None
  */
void spiMasterCsAssert(void);

/**
  * @brief  De-assert the D380 chip-select line (drive PA4 high).
  * @retval None
  */
void spiMasterCsRelease(void);

/**
  * @brief  Transmit a single byte over SPI1 and discard the received byte.
  * @param  byte  Byte to transmit.
  * @retval SpiMasterStatus
  */
SpiMasterStatus spiMasterWriteByte(uint8_t byte);

/**
  * @brief  Receive a single byte over SPI1 (transmits 0xFF as dummy).
  * @param  outByte  Non-NULL pointer to store the received byte.
  * @retval SpiMasterStatus
  */
SpiMasterStatus spiMasterReadByte(uint8_t *outByte);

/**
  * @brief  Transmit and simultaneously receive a buffer over SPI1.
  * @param  txBuf   Pointer to transmit buffer (may be NULL → sends 0xFF).
  * @param  rxBuf   Pointer to receive buffer (may be NULL → discards RX).
  * @param  length  Number of bytes to transfer. Must be > 0.
  * @retval SpiMasterStatus
  */
SpiMasterStatus spiMasterTransfer(const uint8_t *txBuf,
                                   uint8_t       *rxBuf,
                                   uint16_t       length);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_MASTER_H */

/************************ END OF FILE *****************************/

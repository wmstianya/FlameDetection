/**
 * @file    softSpiBitbang.h
 * @brief   Bit-bang SPI (Mode 1) for the ADS1220 on PB3-PB7.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  Added API documentation.
 */
#ifndef SOFT_SPI_BITBANG_H
#define SOFT_SPI_BITBANG_H

#include <stdint.h>

/**
 * @brief  Initialise the bit-bang SPI GPIO (CS/SCK/MOSI out, DRDY/MISO in).
 * @return None.
 */
void softSpiBitbangInit(void);

/**
 * @brief  Clock out one byte, MSB first (Mode 1 timing).
 * @param  data Byte to transmit.
 * @return None.
 */
void softSpiBitbangSendByte(uint8_t data);

/**
 * @brief  Clock in one byte, MSB first (Mode 1 timing).
 * @return Received byte.
 */
uint8_t softSpiBitbangRecvByte(void);

/**
 * @brief  Drive the ADS1220 chip-select (active low).
 * @param  enable Non-zero selects the device (CS low); zero releases it.
 * @return None.
 */
void softSpiBitbangCsSet(uint8_t enable);

#endif /* SOFT_SPI_BITBANG_H */

/**
  ******************************************************************************
  * @file           : crc8.h
  * @brief          : CRC-8 utility (Dallas/Maxim 1-Wire polynomial 0x31)
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Provides CRC-8 calculation compatible with DS18B20 / D380 scratchpad
  * integrity checks and the SPI slave packet protocol.
  *
  * Polynomial: x^8 + x^5 + x^4 + 1  (0x31, reflected: 0x8C)
  * Initial value: 0x00
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __CRC8_H
#define __CRC8_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
  * @brief  Compute CRC-8 over a byte buffer (Dallas/Maxim polynomial 0x31).
  * @param  data    Pointer to input data. Must not be NULL.
  * @param  length  Number of bytes to process. 0 is accepted (returns 0).
  * @retval Computed CRC-8 value.
  */
uint8_t crc8Compute(const uint8_t *data, uint16_t length);

/**
  * @brief  Update a running CRC-8 with one additional byte.
  * @note   Call crc8Compute() for an entire buffer; use this for incremental
  *         computation inside a streaming receive path.
  * @param  crc   Current CRC accumulator value (start with 0x00).
  * @param  byte  Byte to fold into the CRC.
  * @retval Updated CRC-8 value.
  */
uint8_t crc8UpdateByte(uint8_t crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* __CRC8_H */

/************************ END OF FILE *****************************/

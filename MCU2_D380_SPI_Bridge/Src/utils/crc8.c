/**
  ******************************************************************************
  * @file           : crc8.c
  * @brief          : CRC-8 implementation (Dallas/Maxim polynomial 0x31)
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Bit-banged CRC-8 — no look-up table, small Flash footprint.
  * Chosen over a table-driven approach to keep the 256-byte ROM table out of
  * the MCU2's modest 36 KB RAM budget.
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#include "crc8.h"
#include "../config/bridgeConfig.h"

/**
  * @brief  Fold one byte into the running CRC-8 accumulator.
  * @param  crc   Current CRC value (init to 0x00).
  * @param  byte  Byte to process.
  * @retval Updated CRC value.
  */
uint8_t crc8UpdateByte(uint8_t crc, uint8_t byte)
{
    /* XOR byte into current CRC then process each bit */
    crc ^= byte;

    for (uint8_t bit = 0u; bit < 8u; bit++) {
        if (crc & 0x01u) {
            crc = (crc >> 1u) ^ CRC8_POLYNOMIAL;
        } else {
            crc >>= 1u;
        }
    }

    return crc;
}

/**
  * @brief  Compute CRC-8 over an entire byte buffer.
  * @param  data    Pointer to input buffer. Ignored when length == 0.
  * @param  length  Number of bytes to process.
  * @retval Computed CRC-8. Returns 0 when data is NULL or length is 0.
  */
uint8_t crc8Compute(const uint8_t *data, uint16_t length)
{
    uint8_t crc = 0x00u;

    if (data == NULL || length == 0u) {
        return crc;
    }

    for (uint16_t i = 0u; i < length; i++) {
        crc = crc8UpdateByte(crc, data[i]);
    }

    return crc;
}

/************************ END OF FILE *****************************/

/**
  ******************************************************************************
  * @file    tm1650.h
  * @brief   TM1650 4-digit 7-segment LED driver public API.
  *          Also handles LED indicator (PA10) and relay (PA11) GPIO init.
  * @author  Refactored 2026-05-09
  * @date    2026-05-09
  * @version V1.2.0
  *
  * @revision
  *   V1.0.0  2024        Initial version
  *   V1.1.0  2026-05-08  camelCase, IIC ACK fix, function split, doc
  *   V1.2.0  2026-05-09  tm1650Refresh() resends brightness every frame so
  *                        the display auto-recovers after an LED-board
  *                        power glitch (observed on field hardware:
  *                        glitch triggered permanent black-screen because
  *                        brightness was only sent once at boot)
  ******************************************************************************
  */
#ifndef __TM1650_H
#define __TM1650_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define TM1650_DIGIT_COUNT      4U

/**
  * @brief  Initialise TM1650 IIC GPIO (PD1/PD2), LED/Relay GPIO (PA10/PA11),
  *         and send brightness configuration to TM1650.
  * @retval None
  */
void tm1650Init(void);

/**
  * @brief  Write a segment pattern to one digit position via IIC.
  * @param  addr   TM1650 digit register address (0x68/0x6A/0x6C/0x6E)
  * @param  value  7-segment pattern byte
  * @retval 1 on success, 0 on IIC ACK failure
  */
uint8_t tm1650WriteDigit(uint8_t addr, uint8_t value);

/**
  * @brief  Refresh all four digits in one logical frame.  Resends the
  *         brightness / display-on command first so that a TM1650 that
  *         has been power-cycled independently of the MCU (e.g. an
  *         LED-board power glitch) automatically recovers its display.
  * @param  segs  Four-byte array of 7-segment patterns (index 0 is the
  *               leftmost digit).
  * @retval None
  */
void tm1650Refresh(const uint8_t segs[TM1650_DIGIT_COUNT]);

/**
  * @brief  Display a 0-9999 integer on the 4-digit display with
  *         leading-zero blanking.
  * @param  data  Value to display (0 .. 9999)
  * @retval None
  */
void tm1650ShowValue(uint16_t data);

/**
  * @brief  Display a fault code as "ErrN" on the 4-digit display.
  *         Used in FAULT state to give operators a quick diagnostic hint.
  * @param  code  Fault code 1..9 (0 displays "Err0", typically unused)
  * @retval None
  */
void tm1650ShowFaultCode(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* __TM1650_H */

/**
 * @file    tempCalib.h
 * @brief   ADS1220 raw code to furnace temperature (ported from bsp_adc.c).
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  Added API documentation.
 */
#ifndef TEMP_CALIB_H
#define TEMP_CALIB_H

#include <stdint.h>
#include "../config/mcu2Types.h"

/**
 * @brief  Convert a raw ADS1220 sample to furnace temperature.
 * @param  raw    Sign-extended 24-bit ADS1220 sample.
 * @param  readOk Optional; set to 1 for a valid reading, 0 on disconnect/over-range.
 * @return Temperature in degrees C; HOST_TEMP_DISCONNECT on probe fault,
 *         HOST_TEMP_CLAMP_MAX when above the valid range.
 */
uint16 tempCalibRawToTempC(int32_t raw, uint8 *readOk);

#endif /* TEMP_CALIB_H */

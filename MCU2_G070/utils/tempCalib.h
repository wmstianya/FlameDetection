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
 * @brief  Convert a raw ADS1220 sample to the calibration table value.
 * @details Stage 1 of the chain (equivalent to the master Adc_to_temperature()):
 *          raw -> scaling -> table lookup, BEFORE the trim and unit reduction.
 *          Filtering (moving average) is applied to this 0.1 deg C value.
 * @param  raw Sign-extended 24-bit ADS1220 sample.
 * @return Temperature in 0.1 deg C (0 = under-range, ~9999 = over-range).
 */
uint16 tempCalibRawToTenthC(int32_t raw);

/**
 * @brief  Convert a (possibly filtered) 0.1 deg C value to whole-degree output.
 * @details Stage 2 of the chain: empirical -2.5 deg C trim, unit reduction,
 *          over-range clamp and disconnect detection.
 * @param  tenthC Temperature in 0.1 deg C (e.g. from the moving-average filter).
 * @param  readOk Optional; set to 1 for a valid reading, 0 on disconnect/over-range.
 * @return Temperature in degrees C; HOST_TEMP_DISCONNECT on probe fault,
 *         HOST_TEMP_CLAMP_MAX when above the valid range.
 */
uint16 tempCalibTenthToTempC(uint16 tenthC, uint8 *readOk);

/**
 * @brief  Unfiltered convenience conversion: raw ADS1220 sample -> degrees C.
 * @param  raw    Sign-extended 24-bit ADS1220 sample.
 * @param  readOk Optional; set to 1 for a valid reading, 0 on disconnect/over-range.
 * @return Temperature in degrees C (see tempCalibTenthToTempC for sentinels).
 * @note   Equivalent to tempCalibTenthToTempC(tempCalibRawToTenthC(raw), readOk);
 *         the acquisition path filters between the two stages instead.
 */
uint16 tempCalibRawToTempC(int32_t raw, uint8 *readOk);

#endif /* TEMP_CALIB_H */

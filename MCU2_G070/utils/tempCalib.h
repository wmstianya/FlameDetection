/**
 * @file    tempCalib.h
 * @brief   ADS1220 raw to furnace temperature — from bsp_adc.c
 */
#ifndef TEMP_CALIB_H
#define TEMP_CALIB_H

#include <stdint.h>
#include "../config/mcu2Types.h"

uint16 tempCalibRawToTempC(int32_t raw, uint8 *readOk);

#endif /* TEMP_CALIB_H */

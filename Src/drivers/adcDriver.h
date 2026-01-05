/**
  ******************************************************************************
  * @file           : adcDriver.h
  * @brief          : ADC driver abstraction layer
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  * @attention
  * Provides high-level ADC interface with proper error handling.
  * All functions are split to comply with 20-line maximum rule.
  ******************************************************************************
  */

#ifndef __ADC_DRIVER_H
#define __ADC_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "../config/systemConfig.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  ADC operation status codes
  */
typedef enum {
    ADC_STATUS_OK = 0,
    ADC_STATUS_TIMEOUT,
    ADC_STATUS_OUT_OF_RANGE,
    ADC_STATUS_CALIBRATION_FAIL,
    ADC_STATUS_CHANNEL_ERROR
} AdcStatus;

/**
  * @brief  ADC channel identifiers
  */
typedef enum {
    ADC_CHANNEL_FLAME_SENSOR = 1,      /* ADC_CHANNEL_1 */
    ADC_CHANNEL_VREF_INTERNAL = 13,    /* ADC_CHANNEL_VREFINT */
    ADC_CHANNEL_TEMP_SENSOR = 12       /* ADC_CHANNEL_TEMPSENSOR */
} AdcChannelId;

/* Exported functions --------------------------------------------------------*/

/* Initialization */
void adcDriverInit(void);

/* VREFINT and VDDA functions */
AdcStatus adcReadVrefintSafe(uint32_t* value);
float adcCalculateVdda(uint32_t vrefintRaw);
AdcStatus adcGetVddaSafe(float* vdda);

/* Temperature sensor functions */
AdcStatus adcReadTemperatureSafe(int16_t* tempX10);
float adcNormalizeTemperatureData(uint32_t tsData, float vdda);
float adcCalculateTemperature(float tsData3V, uint16_t tsCal1);

/* Flame sensor functions */
AdcStatus adcReadFlameSensor(uint32_t* value);
AdcStatus adcReadFlameSensorAverage(float* avgValue);
float adcConvertToMillivolts(float adcCounts, float vdda);

/* Channel configuration */
AdcStatus adcConfigureChannel(AdcChannelId channelId);
void adcChannelStabilize(uint16_t delayMs);

/* Sampling functions */
AdcStatus adcSampleOnce(uint32_t* value, uint32_t timeoutMs);
AdcStatus adcSampleMultiple(uint32_t* sum, uint8_t count);
uint32_t adcAverageSamples(uint32_t sum, uint8_t count);

/* Debug functions */
void adcDebugVrefint(uint32_t* vrefintAdc, uint16_t* vrefintCal, uint16_t* vddaCalc);
void adcDebugTemperature(uint16_t* tsCal1, uint16_t* tsCal2, uint32_t* tsData, int16_t* tempCalc);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_DRIVER_H */

/************************ END OF FILE *****************************/


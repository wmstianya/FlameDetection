/**
  ******************************************************************************
  * @file           : filter.h
  * @brief          : Digital filter algorithms
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  * @attention
  * Provides various filtering algorithms for signal processing.
  * All functions comply with 20-line maximum rule.
  ******************************************************************************
  */

#ifndef __FILTER_H
#define __FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  IIR filter state structure
  */
typedef struct {
    float alpha;        /* New sample weight (0.0 to 1.0) */
    float beta;         /* Old sample weight (1.0 - alpha) */
    float output;       /* Current filtered output */
    uint8_t initialized; /* Filter initialization flag */
} IirFilter;

/**
  * @brief  Moving average filter structure
  */
typedef struct {
    int16_t* buffer;    /* Circular buffer for samples */
    uint8_t size;       /* Buffer size */
    uint8_t index;      /* Current write position */
    int32_t sum;        /* Running sum */
    uint8_t initialized; /* Filter initialization flag */
} MovingAvgFilter;

/* Exported functions --------------------------------------------------------*/

/* IIR filter functions */
void filterIirInit(IirFilter* filter, float alpha);
float filterIirUpdate(IirFilter* filter, float newSample);
void filterIirReset(IirFilter* filter);

/* Moving average filter functions */
void filterMovingAvgInit(MovingAvgFilter* filter, int16_t* buffer, uint8_t size);
int16_t filterMovingAvgUpdate(MovingAvgFilter* filter, int16_t newSample);
void filterMovingAvgReset(MovingAvgFilter* filter);

/* Simple average functions */
uint32_t filterAverageUint32(const uint32_t* data, uint8_t count);
float filterAverageFloat(const float* data, uint8_t count);

/* Median filter */
uint16_t filterMedianUint16(uint16_t* data, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* __FILTER_H */

/************************ END OF FILE *****************************/


/**
  ******************************************************************************
  * @file           : filter.c
  * @brief          : Digital filter implementations
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  * @attention
  * All filter functions are stateless or use structures to maintain state.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "filter.h"

/* ============================================================================
   IIR Filter Functions
   ============================================================================ */

/**
  * @brief  Initialize IIR filter
  * @param  filter: pointer to filter structure
  * @param  alpha: new sample weight (0.0 to 1.0)
  * @retval None
  */
void filterIirInit(IirFilter* filter, float alpha)
{
    filter->alpha = alpha;
    filter->beta = 1.0f - alpha;
    filter->output = 0.0f;
    filter->initialized = 0;
}

/**
  * @brief  Update IIR filter with new sample
  * @param  filter: pointer to filter structure
  * @param  newSample: new input sample
  * @retval Filtered output
  */
float filterIirUpdate(IirFilter* filter, float newSample)
{
    if (!filter->initialized) {
        filter->output = newSample;
        filter->initialized = 1;
        return filter->output;
    }
    
    filter->output = filter->beta * filter->output + filter->alpha * newSample;
    
    return filter->output;
}

/**
  * @brief  Reset IIR filter
  * @param  filter: pointer to filter structure
  * @retval None
  */
void filterIirReset(IirFilter* filter)
{
    filter->output = 0.0f;
    filter->initialized = 0;
}

/* ============================================================================
   Moving Average Filter Functions
   ============================================================================ */

/**
  * @brief  Initialize moving average filter
  * @param  filter: pointer to filter structure
  * @param  buffer: pointer to sample buffer
  * @param  size: buffer size
  * @retval None
  */
void filterMovingAvgInit(MovingAvgFilter* filter, int16_t* buffer, uint8_t size)
{
    filter->buffer = buffer;
    filter->size = size;
    filter->index = 0;
    filter->sum = 0;
    filter->initialized = 0;
    
    for (uint8_t i = 0; i < size; i++) {
        buffer[i] = 0;
    }
}

/**
  * @brief  Update moving average filter
  * @param  filter: pointer to filter structure
  * @param  newSample: new input sample
  * @retval Filtered output (average)
  */
int16_t filterMovingAvgUpdate(MovingAvgFilter* filter, int16_t newSample)
{
    /* Subtract oldest sample from sum */
    filter->sum -= filter->buffer[filter->index];
    
    /* Add new sample to sum and buffer */
    filter->buffer[filter->index] = newSample;
    filter->sum += newSample;
    
    /* Update index (circular) */
    filter->index++;
    if (filter->index >= filter->size) {
        filter->index = 0;
        filter->initialized = 1;
    }
    
    /* Calculate average */
    if (filter->initialized) {
        return (int16_t)(filter->sum / filter->size);
    } else {
        /* Partial average during initialization */
        return (int16_t)(filter->sum / (filter->index + 1));
    }
}

/**
  * @brief  Reset moving average filter
  * @param  filter: pointer to filter structure
  * @retval None
  */
void filterMovingAvgReset(MovingAvgFilter* filter)
{
    filter->index = 0;
    filter->sum = 0;
    filter->initialized = 0;
    
    for (uint8_t i = 0; i < filter->size; i++) {
        filter->buffer[i] = 0;
    }
}

/* ============================================================================
   Simple Average Functions
   ============================================================================ */

/**
  * @brief  Calculate average of uint32 array
  * @param  data: pointer to data array
  * @param  count: number of samples
  * @retval Average value
  */
uint32_t filterAverageUint32(const uint32_t* data, uint8_t count)
{
    uint32_t sum = 0;
    
    if (count == 0) return 0;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += data[i];
    }
    
    return sum / count;
}

/**
  * @brief  Calculate average of float array
  * @param  data: pointer to data array
  * @param  count: number of samples
  * @retval Average value
  */
float filterAverageFloat(const float* data, uint8_t count)
{
    float sum = 0.0f;
    
    if (count == 0) return 0.0f;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += data[i];
    }
    
    return sum / (float)count;
}

/* ============================================================================
   Median Filter
   ============================================================================ */

/**
  * @brief  Swap two uint16 values
  * @param  a: pointer to first value
  * @param  b: pointer to second value
  * @retval None
  */
static void swapUint16(uint16_t* a, uint16_t* b)
{
    uint16_t temp = *a;
    *a = *b;
    *b = temp;
}

/**
  * @brief  Sort uint16 array (bubble sort for small arrays)
  * @param  data: pointer to data array
  * @param  count: number of elements
  * @retval None
  */
static void sortUint16Array(uint16_t* data, uint8_t count)
{
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            if (data[j] > data[j + 1]) {
                swapUint16(&data[j], &data[j + 1]);
            }
        }
    }
}

/**
  * @brief  Calculate median of uint16 array
  * @param  data: pointer to data array (will be modified)
  * @param  count: number of samples
  * @retval Median value
  */
uint16_t filterMedianUint16(uint16_t* data, uint8_t count)
{
    if (count == 0) return 0;
    
    sortUint16Array(data, count);
    
    if (count % 2 == 1) {
        return data[count / 2];
    } else {
        return (data[count / 2 - 1] + data[count / 2]) / 2;
    }
}

/************************ END OF FILE *****************************/


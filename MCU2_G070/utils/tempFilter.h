/**
 * @file    tempFilter.h
 * @brief   21-point moving-average filter for furnace temperature (0.1 deg C).
 * @details Mirrors the master WenDu_Filter_Function(): smooths the calibration
 *          table output before the trim / unit reduction. Runs in the ADS1220
 *          ACQUISITION path (one push per ~50 ms sample), which is decoupled
 *          from the 1 Hz host SPI polling.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.0.0
 */
#ifndef TEMP_FILTER_H
#define TEMP_FILTER_H

#include "../config/mcu2Types.h"

/** Moving-average window length (matches the master's 21-point filter). */
#define TEMP_FILTER_WINDOW      21U

/**
 * @brief  Moving-average filter state (encapsulated; no loose globals).
 */
typedef struct
{
    uint16 samples[TEMP_FILTER_WINDOW]; /**< Ring buffer of recent samples.   */
    uint32 sum;                         /**< Running sum of stored samples.   */
    uint8  count;                       /**< Stored samples (<= window).      */
    uint8  head;                        /**< Next ring-buffer write index.    */
} TempFilter;

/**
 * @brief  Reset the filter to empty.
 * @param  filter Filter state (must not be NULL).
 * @return None.
 */
void tempFilterInit(TempFilter *filter);

/**
 * @brief  Push one sample and return the current moving average.
 * @param  filter Filter state (must not be NULL).
 * @param  sample New sample in 0.1 deg C.
 * @return Average of the last up-to-TEMP_FILTER_WINDOW samples (0.1 deg C).
 */
uint16 tempFilterPush(TempFilter *filter, uint16 sample);

#endif /* TEMP_FILTER_H */

/**
 * @file    tempFilter.c
 * @brief   21-point moving-average filter for furnace temperature (0.1 deg C).
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.0.0
 */
#include "tempFilter.h"

void tempFilterInit(TempFilter *filter)
{
    uint8 i;
    if (filter == NULL)
        return;
    for (i = 0U; i < TEMP_FILTER_WINDOW; i++)
        filter->samples[i] = 0U;
    filter->sum = 0U;
    filter->count = 0U;
    filter->head = 0U;
}

uint16 tempFilterPush(TempFilter *filter, uint16 sample)
{
    if (filter == NULL)
        return sample;

    /* Replace the oldest slot: subtract its stored value, add the new one. The
     * subtracted slot is 0 until the window fills, so the average tracks the
     * partial count and is responsive from the very first sample. */
    filter->sum -= filter->samples[filter->head];
    filter->sum += sample;
    filter->samples[filter->head] = sample;

    filter->head = (uint8)((filter->head + 1U) % TEMP_FILTER_WINDOW);
    if (filter->count < TEMP_FILTER_WINDOW)
        filter->count++;

    return (uint16)(filter->sum / filter->count);
}

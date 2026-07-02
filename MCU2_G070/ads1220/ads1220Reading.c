/**
 * @file    ads1220Reading.c
 * @brief   Pure fault/staleness policy for ADS1220 furnace-temperature reads.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.0.0  Initial: A1 dead-sensor detection.
 */
#include "ads1220Reading.h"
#include "../protocol/hostProtocol.h"

void ads1220ReadingInit(Ads1220Reading *state, uint16 seedTempC)
{
    if (state == NULL)
        return;
    state->lastGoodTempC = seedTempC;
    state->lastGoodMs = 0U;
    state->hasGood = 0U;
}

/**
 * @brief  True while a not-ready read may still hold the last good value.
 * @details Uses unsigned subtraction so the 32-bit millisecond tick rollover
 *          (~49.7 days) is handled correctly. Before the first valid read the
 *          window is measured from tick 0 (start-up), giving a bounded warm-up.
 */
static uint8 ads1220ReadingWithinHoldWindow(const Ads1220Reading *state,
                                             uint32 nowMs)
{
    uint32 elapsedMs = (uint32)(nowMs - state->lastGoodMs);
    return (elapsedMs <= ADS1220_STALE_TIMEOUT_MS) ? 1U : 0U;
}

uint16 ads1220ReadingResolve(Ads1220Reading *state, uint8 readStatus,
                             uint16 convTempC, uint8 convValid,
                             uint32 nowMs, uint8 *statOut)
{
    if (state == NULL)
    {
        if (statOut != NULL)
            *statOut = HOST_STAT_FAULT;
        return HOST_TEMP_DISCONNECT;
    }

    if ((readStatus == ADS1220_PORT_OK) && (convValid != 0U))
    {
        state->lastGoodTempC = convTempC;
        state->lastGoodMs = nowMs;
        state->hasGood = 1U;
        if (statOut != NULL)
            *statOut = HOST_STAT_OK;
        return convTempC;
    }

    if ((readStatus == ADS1220_PORT_NOT_READY) &&
        (ads1220ReadingWithinHoldWindow(state, nowMs) != 0U))
    {
        if (statOut != NULL)
            *statOut = HOST_STAT_OK;
        return state->lastGoodTempC;
    }

    if (statOut != NULL)
        *statOut = HOST_STAT_FAULT;
    return HOST_TEMP_DISCONNECT;
}

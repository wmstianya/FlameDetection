/**
 * @file    tempCalib.c
 * @brief   Lookup-table temperature conversion from bsp_adc.c adcProcessSmokeAds1220
 */
#include "tempCalib.h"
#include "../protocol/hostProtocol.h"

static const uint16 kAdcCalib[221] = {
    10000, 10039, 10078, 10117, 10156, 10195, 10234, 10273, 10312, 10351,
    10390, 10429, 10468, 10507, 10546, 10585, 10624, 10663, 10702, 10740,
    10779, 10818, 10857, 10896, 10935, 10973, 11012, 11051, 11090, 11129,
    11167, 11206, 11245, 11283, 11322, 11361, 11400, 11438, 11477, 11515,
    11554, 11593, 11631, 11670, 11708, 11747, 11786, 11824, 11863, 11901,
    11940, 11978, 12017, 12055, 12094, 12132, 12171, 12209, 12247, 12286,
    12324, 12363, 12401, 12439, 12478, 12516, 12554, 12593, 12631, 12669,
    12708, 12746, 12784, 12822, 12861, 12899, 12937, 12975, 13013, 13050,
    13090, 13128, 13166, 13204, 13242, 13280, 13318, 13357, 13395, 13433,
    13471, 13509, 13547, 13585, 13623, 13661, 13699, 13737, 13775, 13813,
    13851, 13888, 13926, 13964, 14002, 14040, 14078, 14116, 14154, 14191,
    14229, 14267, 14305, 14343, 14380, 14418, 14456, 14494, 14531, 14569,
    14607, 14644, 14682, 14720, 14757, 14795, 14833, 14870, 14908, 14946,
    14983, 15021, 15058, 15096, 15133, 15171, 15208, 15246, 15283, 15321,
    15358, 15396, 15433, 15471, 15508, 15546, 15583, 15620, 15658, 15695,
    15733, 15770, 15807, 15845, 15882, 15919, 15956, 16031, 16068, 16105,
    16105, 16143, 16180, 16217, 16254, 16291, 16329, 16366, 16403, 16440,
    16477, 16514, 16551, 16589, 16626, 16663, 16700, 16737, 16774, 16811,
    16848, 16885, 16922, 16959, 16996, 17033, 17070, 17107, 17143, 17180,
    17217, 17254, 17291, 17328, 17365, 17402, 17438, 17475, 17512, 17549,
    17586, 17622, 17659, 17696, 17733, 17769, 17806, 17843, 17879, 17916,
    17953, 17989, 18026, 18063, 18099, 18136, 18172, 18209, 18246, 18282,
    18319,
};

static uint16 tempCalibGetDot(uint16 value, uint16 span)
{
    if (span == 0U)
        return 0U;
    return (uint16)((value * 10U) / span);
}

static uint16 tempCalibDecadeLookup(uint16 value, uint8 decade)
{
    uint16 base = (uint16)(decade * 100U);
    const uint16 *row = &kAdcCalib[decade * 10U];
    uint8 i;

    for (i = 0U; i < 10U; i++)
    {
        uint16 lo = row[i];
        uint16 hi = row[i + 1U];
        if (value >= lo && value < hi)
            return (uint16)(base + (uint16)(i * 10U) +
                            tempCalibGetDot((uint16)(value - lo), (uint16)(hi - lo)));
    }
    return 0U;
}

static uint16 tempCalibFromQuantized(uint16 value)
{
    uint8 decade;

    if (value > 9126U && value < kAdcCalib[0])
        return 0U;
    for (decade = 0U; decade <= 20U; decade++)
    {
        uint16 lo = kAdcCalib[decade * 10U];
        uint16 hi = kAdcCalib[(decade + 1U) * 10U];
        if (value >= lo && value < hi)
            return tempCalibDecadeLookup(value, decade);
    }
    return 9999U;
}

uint16 tempCalibRawToTempC(int32_t raw, uint8 *readOk)
{
    float scaled;
    uint32_t quantized;
    uint16 lookupTenthC;
    uint16 tempC;

    if (raw < 0)
        raw = 0;
    scaled = ((float)raw / 8388607.0f) * 2000.0f;
    scaled = scaled / 16.0f / 0.5f;
    quantized = (uint32_t)(scaled * 100.0f);
    lookupTenthC = tempCalibFromQuantized((uint16)quantized);
    if (lookupTenthC > 100U)
        lookupTenthC = (uint16)(lookupTenthC - 25U);
    if (lookupTenthC > 9000U)
    {
        if (readOk != NULL)
            *readOk = 0U;
        return HOST_TEMP_DISCONNECT;
    }
    tempC = (uint16)(lookupTenthC / 10U);
    if (tempC > 390U)
        tempC = HOST_TEMP_CLAMP_MAX;
    if (readOk != NULL)
        *readOk = 1U;
    return tempC;
}

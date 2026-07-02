/**
 * @file    tempCalib.c
 * @brief   Lookup-table furnace-temperature conversion (ADS1220 raw -> deg C).
 * @details Ported from the D380 master bsp_adc.c adcProcessSmokeAds1220(); the
 *          table, scaling factors and empirical trim are kept numerically
 *          identical to the master so both MCUs agree on the temperature.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  De-magic: physics/threshold constants named; math unchanged.
 */
#include "tempCalib.h"
#include "../protocol/hostProtocol.h"

/* --- ADS1220 front-end scaling (drives raw code -> millivolt -> table code) --- */
#define ADC_FULL_SCALE_CODE       8388607.0f /* 2^23 - 1: 24-bit signed full scale  */
#define ADC_VREF_MILLIVOLT        2000.0f    /* external reference, millivolts       */
#define ADC_PGA_GAIN              16.0f      /* Reg0 PGA gain = x16                   */
#define ADC_FRONTEND_DIVIDER      0.5f       /* sensor front-end divide (bsp_adc.c)   */
#define CALIB_CODE_CENTI_SCALE    100.0f     /* scale to the table's centi-unit code  */

/* --- Calibration table geometry (kAdcCalib) --- */
#define CALIB_TABLE_LEN           221U       /* 22 decades * 10 + 1 boundary entry    */
#define CALIB_ENTRIES_PER_DECADE  10U        /* codes per 100 * 0.1 deg C decade row  */
#define CALIB_MAX_DECADE_INDEX    20U        /* highest decade scanned (see loop)     */
#define CALIB_TENTHC_PER_DECADE   100U       /* 0.1 deg C units spanned by one decade */
#define CALIB_TENTHC_PER_STEP     10U        /* 0.1 deg C units per intra-decade step */
#define CALIB_INTERP_SCALE        10U        /* fixed-point scale for sub-step interp */
#define CALIB_UNDERRANGE_CODE     9126U      /* codes in (this, table[0]) -> 0 deg C   */
#define CALIB_OVERRANGE_TENTHC    9999U      /* code above the table -> over-range     */

/* --- Result post-processing (all in 0.1 deg C unless noted) --- */
#define CALIB_OFFSET_GATE_TENTHC  100U       /* apply trim only above 10.0 deg C       */
#define CALIB_OFFSET_TRIM_TENTHC  25U        /* empirical -2.5 deg C trim (bsp_adc.c)  */
#define TEMP_TENTHC_DISCONNECT    9000U      /* > 900.0 deg C => probe disconnected    */
#define TENTHC_PER_DEGREE         10U        /* 0.1 deg C units per whole degree        */
#define TEMP_VALID_MAX_C          390U       /* above => clamp to HOST_TEMP_CLAMP_MAX   */

static const uint16 kAdcCalib[CALIB_TABLE_LEN] = {
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

/**
 * @brief  Linear sub-step interpolation, returning the 0.1 deg C remainder.
 * @param  value Offset of the code above the lower table entry.
 * @param  span  Code distance between the two bracketing table entries.
 * @return Interpolated fraction in [0, CALIB_INTERP_SCALE) as 0.1 deg C units.
 */
static uint16 tempCalibGetDot(uint16 value, uint16 span)
{
    if (span == 0U)
        return 0U;
    return (uint16)((value * CALIB_INTERP_SCALE) / span);
}

/**
 * @brief  Resolve a code within one decade row into 0.1 deg C.
 * @param  value  Quantized ADC code known to fall inside this decade.
 * @param  decade Decade index (row = decade * CALIB_ENTRIES_PER_DECADE).
 * @return Temperature in 0.1 deg C, or 0 if no bracketing pair matched.
 */
static uint16 tempCalibDecadeLookup(uint16 value, uint8 decade)
{
    uint16 base = (uint16)(decade * CALIB_TENTHC_PER_DECADE);
    const uint16 *row = &kAdcCalib[decade * CALIB_ENTRIES_PER_DECADE];
    uint8 i;

    for (i = 0U; i < CALIB_ENTRIES_PER_DECADE; i++)
    {
        uint16 lo = row[i];
        uint16 hi = row[i + 1U];
        if (value >= lo && value < hi)
            return (uint16)(base + (uint16)(i * CALIB_TENTHC_PER_STEP) +
                            tempCalibGetDot((uint16)(value - lo), (uint16)(hi - lo)));
    }
    return 0U;
}

/**
 * @brief  Map a quantized ADC code to 0.1 deg C via the calibration table.
 * @param  value Quantized ADC code (see tempCalibRawToTempC scaling).
 * @return 0 when just below the table, CALIB_OVERRANGE_TENTHC when above it,
 *         otherwise the interpolated temperature in 0.1 deg C.
 */
static uint16 tempCalibFromQuantized(uint16 value)
{
    uint8 decade;

    if (value > CALIB_UNDERRANGE_CODE && value < kAdcCalib[0])
        return 0U;
    for (decade = 0U; decade <= CALIB_MAX_DECADE_INDEX; decade++)
    {
        uint16 lo = kAdcCalib[decade * CALIB_ENTRIES_PER_DECADE];
        uint16 hi = kAdcCalib[(decade + 1U) * CALIB_ENTRIES_PER_DECADE];
        if (value >= lo && value < hi)
            return tempCalibDecadeLookup(value, decade);
    }
    return CALIB_OVERRANGE_TENTHC;
}

uint16 tempCalibRawToTenthC(int32_t raw)
{
    float scaled;
    uint32_t quantized;

    if (raw < 0)
        raw = 0;
    /* raw code -> input millivolts -> sensor units -> table's centi-unit code. */
    scaled = ((float)raw / ADC_FULL_SCALE_CODE) * ADC_VREF_MILLIVOLT;
    scaled = scaled / ADC_PGA_GAIN / ADC_FRONTEND_DIVIDER;
    quantized = (uint32_t)(scaled * CALIB_CODE_CENTI_SCALE);

    return tempCalibFromQuantized((uint16)quantized);
}

uint16 tempCalibTenthToTempC(uint16 tenthC, uint8 *readOk)
{
    uint16 tempC;

    /* Empirical calibration trim carried over from the master bsp_adc.c: above
     * 10.0 deg C the table reads ~2.5 deg C high, so subtract the fixed trim.
     * Do NOT change without re-characterising against a reference thermometer. */
    if (tenthC > CALIB_OFFSET_GATE_TENTHC)
        tenthC = (uint16)(tenthC - CALIB_OFFSET_TRIM_TENTHC);

    if (tenthC > TEMP_TENTHC_DISCONNECT)
    {
        if (readOk != NULL)
            *readOk = 0U;
        return HOST_TEMP_DISCONNECT;
    }
    tempC = (uint16)(tenthC / TENTHC_PER_DEGREE);
    if (tempC > TEMP_VALID_MAX_C)
        tempC = HOST_TEMP_CLAMP_MAX;
    if (readOk != NULL)
        *readOk = 1U;
    return tempC;
}

uint16 tempCalibRawToTempC(int32_t raw, uint8 *readOk)
{
    return tempCalibTenthToTempC(tempCalibRawToTenthC(raw), readOk);
}

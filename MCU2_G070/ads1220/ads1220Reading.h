/**
 * @file    ads1220Reading.h
 * @brief   Pure fault/staleness policy for ADS1220 furnace-temperature reads.
 * @details Isolated from the HAL bit-bang port so the safety-critical logic
 *          (dead-sensor detection, hold-last-good window) is host-unit-testable.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.0.0  Initial: adds A1 dead-sensor detection (time-based staleness).
 */
#ifndef ADS1220_READING_H
#define ADS1220_READING_H

#include "../config/mcu2Types.h"
#include "ads1220Port.h"   /* ADS1220_PORT_OK / _NOT_READY / _ERROR status codes */

/**
 * Max time a not-ready ADS1220 may hold its last good value before the
 * reading is declared stale (dead / disconnected sensor). The device runs in
 * continuous conversion mode at 20 SPS (~50 ms), so this 1 s window is ~20x
 * the conversion period: large enough to never false-trip on a healthy part,
 * small enough that a furnace protection host learns of a dead probe quickly.
 */
#define ADS1220_STALE_TIMEOUT_MS      1000U

/**
 * @brief Rolling state for one ADS1220 channel's reading policy.
 * @note  Encapsulated in a struct (no loose globals) per project standard.
 */
typedef struct
{
    uint16 lastGoodTempC;   /**< Last valid furnace temperature, degrees C. */
    uint32 lastGoodMs;      /**< Millisecond tick of the last valid read.   */
    uint8  hasGood;         /**< 0 until the first valid read is seen.      */
} Ads1220Reading;

/**
 * @brief  Initialise reading state with a start-up seed value.
 * @param  state     Reading state to initialise (must not be NULL).
 * @param  seedTempC Provisional temperature reported during warm-up window.
 * @return None.
 */
void ads1220ReadingInit(Ads1220Reading *state, uint16 seedTempC);

/**
 * @brief  Resolve one read attempt into a furnace temperature and host status.
 * @details Policy:
 *          - Valid conversion            -> update state, STAT_OK, return temp.
 *          - Not-ready within timeout     -> STAT_OK, hold last good (transient
 *            gap between conversions is normal in continuous mode).
 *          - Not-ready beyond timeout, an invalid conversion, or a hard error
 *            -> STAT_FAULT and a >=900 disconnect sentinel so the F103 host
 *            trips its 断线 / over-temperature path instead of trusting a
 *            frozen value (this is the A1 dead-sensor fix).
 * @param  state     Reading state (must not be NULL).
 * @param  readStatus One of ADS1220_PORT_OK / _NOT_READY / _ERROR.
 * @param  convTempC  Converted temperature (only used when readStatus==OK).
 * @param  convValid  Non-zero when the conversion produced a valid in-range value.
 * @param  nowMs      Current millisecond tick (monotonic; wrap-safe).
 * @param  statOut    Optional; receives HOST_STAT_OK or HOST_STAT_FAULT.
 * @return Furnace temperature in degrees C (or a >=900 sentinel on fault).
 */
uint16 ads1220ReadingResolve(Ads1220Reading *state, uint8 readStatus,
                             uint16 convTempC, uint8 convValid,
                             uint32 nowMs, uint8 *statOut);

#endif /* ADS1220_READING_H */

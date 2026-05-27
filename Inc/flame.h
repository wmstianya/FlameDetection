/**
  ******************************************************************************
  * @file    flame.h
  * @brief   Industrial-grade flame detection module: ADC sampling, trimmed-
  *          mean filter, optional false-flame (Err6) gate, fixed startup
  *          baseline, and a state machine governing the LED indicator and
  *          fuel-cutoff relay.
  *
 * @author  2026-05-27
 * @date    2026-05-27
 * @version V1.5.0
 *
 * @revision
 *   V1.0.0  2026-05-08  Initial version (extracted from main.c, hardened
 *                        with VREFINT calibration, fault gate, and
 *                        adaptive baseline)
 *   V1.2.0  2026-05-09  Post-field-test hardening: relative-short gate,
 *                        sticky FAULT, baseline sanity guard, new
 *                        Err6 (SHORT_REL) code.  See flame.c revision.
 *   V1.3.0  2026-05-09  Ion-probe field-hardening: 50 Hz notch, ignition
 *                        frame discard, baseline drift monitor (Err7).
 *   V1.4.0  2026-05-11  Field-data rebalance: FAULT_FLOOR_MV 500->250 mV,
 *                        variance-gated short detection throughout, new
 *                        absolute-level Err6 that fires during
 *                        WAIT_BASELINE (handles boot-time condensate).
 *   V1.5.0  2026-05-27  Simplified detection: fixed startup baseline,
 *                        500 mV drop for FLAME_ON, FLAME_OFF at startup
 *                        baseline with debounce.  Only Err6 retained
 *                        (optional via ENABLE_VARIANCE_CHECK).
  ******************************************************************************
  */
#ifndef __FLAME_H
#define __FLAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Flame detection state.
  *
  *         Power-on transitions:
  *           BLANKING --(2 s)--> WAIT_BASELINE --(16 plausible samples)-->
  *               NO_FLAME <--> FLAME_ON <--> CONFIRM_OFF
  *
  *         Any state may be pre-empted by FAULT (highest priority).
  *         FAULT is sticky: no transition out of FAULT exists.  A reset
  *         (watchdog / power cycle) puts the state machine back into
  *         BLANKING, but if the underlying short/open is still present the
  *         baseline-sanity guard keeps the unit looping in WAIT_BASELINE
  *         and IWDG keeps resetting it, so the relay never closes until
  *         the probe is physically corrected.  This is a reset-cycle
  *         lock-out, not a true UL-296 manual-reset latch (which would
  *         require VBAT-backed RTC BKP or Flash persistence).
  */
typedef enum
{
    FLAME_STATE_BLANKING      = 0,
    FLAME_STATE_WAIT_BASELINE = 1,
    FLAME_STATE_NO_FLAME      = 2,
    FLAME_STATE_FLAME_ON      = 3,
    FLAME_STATE_CONFIRM_OFF   = 4,
    FLAME_STATE_FAULT         = 5
} flameState_t;

/**
  * @brief  Initialise flame detection module.  Must be called after MCU
  *         clock + ADC init and after tm1650Init() (which performs the
  *         LED / relay GPIO configuration).
  * @retval None
  */
void flameInit(void);

/**
  * @brief  One full processing cycle: collect 128 samples (~128 ms), apply
  *         trimmed-mean filter, evaluate fault gate, run state machine,
  *         drive relay/LED, and update display.  Call from main loop.
  * @retval None
  */
void flameProcess(void);

/**
  * @brief  1 ms tick callback, invoked from SysTick_Handler.
  *         Drives flameOffCount / firstStartCount and the FAULT-state
  *         LED blink.
  * @retval None
  */
void flameTickCallback(void);

/* ---- Diagnostic getters (read-only) -------------------------------------- */

flameState_t flameGetState(void);
uint16_t     flameGetLastMv(void);
uint16_t     flameGetLastVariance(void);  /* diagnostic: last trimmed variance */
uint16_t     flameGetBaselineMv(void);    /* fixed startup baseline (mV)       */
uint8_t      flameGetFaultCode(void);   /* 0 = no fault, otherwise FAULT_CODE_* */
int16_t      flameGetDieTempC(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLAME_H */

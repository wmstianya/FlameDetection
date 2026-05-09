/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.  Common defines and tuning
  *                   constants for the FlameDetection application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include "stm32g0xx_ll_system.h"

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* Defined in flame.c -- called from SysTick_Handler in stm32g0xx_it.c */
void flameTickCallback(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* ===== ADC sampling / filtering ========================================== */
#define ADC_SAMPLE_COUNT        128U          /* samples per main cycle      */
#define ADC_FULL_SCALE          4095U         /* 12-bit ADC                  */
#define TRIM_TOP_COUNT          8U            /* samples discarded high end  */
#define TRIM_BOTTOM_COUNT       8U            /* samples discarded low  end  */
#define TRIMMED_SAMPLE_COUNT    (ADC_SAMPLE_COUNT - TRIM_TOP_COUNT - TRIM_BOTTOM_COUNT)

/* ===== Timing ============================================================ */
#define STARTUP_DELAY_MS        2000U         /* BLANKING state duration     */
#define FLAME_OFF_TIMEOUT_MS    1500U         /* CONFIRM_OFF -> NO_FLAME     */

/* ===== ADC trigger / refresh cadence ==================================== */
#define ADC_TRIG_FREQ_HZ        1000U         /* TIM6 TRGO -> ADC trigger    */
#define VDDA_REFRESH_PERIOD     8U            /* refresh VDDA+dieTemp every  */
                                              /* N frames (~1 s @ 128ms)     */

/* ===== Adaptive baseline (Schmitt-trigger relative thresholds) =========== */
#define FLAME_ON_DELTA_MV       100U          /* mV below baseline => ON     */
#define FLAME_OFF_DELTA_MV       60U          /* mV below baseline => stay   */
#define BASELINE_EMA_SHIFT      5U            /* tau ~ 32 cycles ~ 4 s       */
#define BASELINE_INIT_SAMPLES   16U           /* averaged at WAIT_BASELINE   */

/* ===== Fault-detection gate (industrial safety, fail-safe) =============== *
 *
 * Post-field-test hardening: the absolute FLOOR_MV check leaked on wet-probe
 * shorts that settled above 500 mV but well below baseline.  We now use two
 * independent shorting checks:
 *   (a) Absolute floor (Err1): catches dead shorts to ground.
 *   (b) Relative drop + low variance (Err6): baseline-mv > SHORT_DROP_MV AND
 *       variance < MIN_FLAME_VARIANCE.  Catches wet/partial shorts that stay
 *       above the absolute floor but are physically impossible given baseline.
 *
 * SAFETY POLICY: the FAULT state is sticky until the unit power-cycles.
 * This is a RESET-CYCLE LOCK-OUT -- not a UL-296 manual-reset latch, which
 * would require persistent storage (RTC BKP or Flash) and appropriate
 * hardware (VBAT) that this PCB does not currently provide.  In addition,
 * accumulateBaseline() rejects physically-implausible seed values, so a
 * persistent short at boot forces the system into an IWDG reset loop
 * rather than silently adopting the short as the new baseline.
 */
#define FAULT_FLOOR_MV          500U          /* < this => Err1 (dead short)      */
#define FAULT_CEILING_MV        3200U         /* > this => Err2 (open circuit)    */
#define SHORT_DROP_MV           800U          /* baseline - mv > this + low var   */
                                              /*           => Err6 (wet/partial)  */
#define VDDA_MIN_MV             2700U         /* Err3 if VDDA out of range        */
#define VDDA_MAX_MV             3600U
#define MIN_FLAME_VARIANCE      4U            /* < => Err4 (signal too dead)      */
#define MAX_DV_PER_CYCLE_MV     1500U         /* > => suspicious jump             */
#define DV_FAULT_CONFIRM_COUNT  3U            /* consecutive jumps -> Err5        */

/* Baseline sanity guard used by WAIT_BASELINE seeding.  Anything outside
 * this window at power-up is assumed to be a stuck input (short / open) and
 * is refused -- the state machine stays in WAIT_BASELINE forever, IWDG
 * eventually resets, and the operator has to physically rectify the probe. */
#define BASELINE_SANE_MIN_MV    1500U
#define BASELINE_SANE_MAX_MV    3000U

/* ===== Fault codes (displayed as ErrN on TM1650) ========================= */
#define FAULT_CODE_NONE         0U
#define FAULT_CODE_FLOOR        1U            /* Err1 dead short to ground   */
#define FAULT_CODE_CEILING      2U            /* Err2 open circuit           */
#define FAULT_CODE_VDDA         3U            /* Err3 VDDA out of range      */
#define FAULT_CODE_VARIANCE     4U            /* Err4 signal too dead        */
#define FAULT_CODE_JUMP         5U            /* Err5 non-physical jump      */
#define FAULT_CODE_SHORT_REL    6U            /* Err6 wet/partial short      */
#define FAULT_CODE_DRIFT        7U            /* Err7 probe soot/moisture    */

/* ===== 50 Hz notch filter ================================================ */
/* IIR biquad notch: fs=1000 Hz, f0=50 Hz, Q=5 (BW ~10 Hz).
 * Removes mains EMI picked up by the high-impedance ion probe without
 * affecting the 1~15 Hz flame-flicker band. */

/* ===== Ignition spark frame discard ====================================== */
#define ADC_CLIP_LOW            10U           /* < this => clipped to ground */
#define ADC_CLIP_HIGH           4085U         /* > this => clipped to rail   */
#define CLIP_DISCARD_THRESHOLD  16U           /* > this many clips -> discard*/

/* ===== Baseline drift monitoring (probe soot / moisture) ================= *
 * If baseline drifts down by more than BASELINE_DRIFT_WARN_MV over a
 * DRIFT_CHECK_INTERVAL window, the probe is accumulating soot or moisture
 * and needs maintenance.  Reports Err7 and latches FAULT. */
#define BASELINE_DRIFT_WARN_MV  300U          /* mV cumulative drop -> Err7  */
#define DRIFT_CHECK_INTERVAL    480U          /* frames (~60 s @ 128 ms)     */

/* ===== Watchdog ========================================================== */
#define IWDG_RELOAD_VALUE       4095U
#define IWDG_WINDOW_VALUE       4095U         /* 4095 = window disabled      */

/* ===== GPIO pin map ====================================================== */
#define LED_PORT                GPIOA
#define LED_PIN                 GPIO_PIN_10
#define RELAY_PORT              GPIOA
#define RELAY_PIN               GPIO_PIN_11

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

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

/* ===== Fixed startup baseline (V1.5.0) =================================== */
#define FLAME_ON_DELTA_MV       500U          /* startupBaseline - mv > this   */
                                              /* => FLAME_ON (relay closes)     */
#define BASELINE_INIT_SAMPLES   16U           /* averaged at WAIT_BASELINE     */

/* Optional false-flame (Err6) variance gate.  Production default = 0 (off).
 * Set to 1 to enable: large drop + variance <= FALSE_FLAME_VAR_MAX => Err6. */
#define ENABLE_VARIANCE_CHECK   0U            /* 1 = on, 0 = off (production) */
#define FALSE_FLAME_VAR_MAX     100U          /* LSB^2; var <= this => Err6    */

/* Baseline sanity guard used by WAIT_BASELINE seeding.  Anything outside
 * this window at power-up is assumed to be a stuck input (short / open) and
 * is refused -- the state machine stays in WAIT_BASELINE forever, IWDG
 * eventually resets, and the operator has to physically rectify the probe. */
#define BASELINE_SANE_MIN_MV    1500U
#define BASELINE_SANE_MAX_MV    3000U

/* SAFETY POLICY: the FAULT state is sticky until the unit power-cycles. */

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

/* ===== Watchdog ========================================================== */
#define IWDG_RELOAD_VALUE       4095U
#define IWDG_WINDOW_VALUE       4095U         /* 4095 = window disabled      */

/* ===== Diagnostic display (development only) ============================ *
 * DIAG_SHOW_VARIANCE=1: display shows variance continuously (with trailing
 * decimal point), but flips to mV for DIAG_MV_FRAMES (~1 s) once every
 * DIAG_CYCLE_FRAMES (~5 s).  Variance > 9999 is compressed by /10 to fit
 * the 4-digit field.  Set DIAG_SHOW_VARIANCE to 0 for production firmware.
 *
 * DISABLE_FAULT_GATE=1: bypasses evaluateFaultGate so the fault-code display
 * never triggers.  TEST MODE ONLY -- do NOT ship. */
#define DIAG_SHOW_VARIANCE      0U            /* 1 = diag display, 0 = off     */
#define DIAG_CYCLE_FRAMES       39U           /* 5 s total cycle (39 × 128 ms) */
#define DIAG_MV_FRAMES          8U            /* 1 s mV window per cycle       */
#define DISABLE_FAULT_GATE      0U            /* 1 = bypass fault gate (TEST)  */

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

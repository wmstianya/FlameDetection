/**
  ******************************************************************************
  * @file    adc.h
  * @brief   Non-blocking ADC driver: TIM6 1 kHz triggers ADC1, results pushed
  *          to a 128-sample buffer via DMA1 channel 1.  Application waits
  *          for a per-frame TC interrupt instead of busy-looping HAL_Delay.
  *
  *          Internal channels (VREFINT, TEMPSENSOR) are read on demand via
  *          a brief Stop_DMA -> ConfigChannel(internal) -> polling read ->
  *          ConfigChannel(CH1) sequence.  The caller is expected to re-arm
  *          the next frame with adcStartFrame() afterwards.
  *
  * @author  2026-05-08
  * @date    2026-05-08
  * @version V1.3.0
  *
  * @revision
  *   V1.0.0  2022        Initial CubeMX-generated polling version
  *   V1.1.0  2026-05-08  Hide hadc handle, expose adcConvertSingle API
  *   V1.2.0  2026-05-08  Add VREFINT/TEMPSENSOR internal channels and HW
  *                        oversampling
  *   V1.3.0  2026-05-08  Move to TIM6-triggered DMA, non-blocking frame API
  *   V1.3.1  2026-05-09  Unify VREFINT+TEMPSENSOR read as adcReadDiagnostics
  *                        (single DMA-pause window, preserves ADC calibration
  *                        via CFGR1 direct write instead of HAL_ADC_Init)
  ******************************************************************************
  */
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Bring up DMA1 channel 1, ADC1 (with TIM6 TRGO trigger and 4x
  *         hardware oversampling), and TIM6 base.  Starts TIM6 but does
  *         NOT start the first DMA frame -- the caller fires that with
  *         adcStartFrame() after upper layers (flame.c) are initialised.
  * @retval None
  */
void MX_ADC1_Init(void);

/**
  * @brief  Arm DMA + ADC for one frame of ADC_SAMPLE_COUNT samples.
  *         TIM6 TRGO drives the conversion cadence; DMA writes results
  *         into the internal sample buffer.  At completion, the ADC TC
  *         callback sets the frame-ready flag.
  * @retval None
  */
void adcStartFrame(void);

/**
  * @brief  Non-blocking poll of frame readiness.
  * @retval 1 if a full frame of samples is available, 0 if still in flight
  */
uint8_t adcFrameReady(void);

/**
  * @brief  Return a read-only pointer to the latest sample buffer.
  *         Length = ADC_SAMPLE_COUNT uint16_t.  Valid only between
  *         adcFrameReady() returning 1 and the next adcStartFrame().
  * @retval Pointer to sample buffer
  */
const uint16_t *adcGetBuffer(void);

/**
  * @brief  Clear the frame-ready flag.  Caller is expected to have
  *         consumed/copied adcGetBuffer() before calling this.
  * @retval None
  */
void adcClearFrameFlag(void);

/**
  * @brief  Accessor for the DMA handle backing ADC1.  Required by
  *         DMA1_Channel1_IRQHandler in stm32g0xx_it.c.
  * @retval Pointer to internal hdma_adc handle
  */
DMA_HandleTypeDef *adcGetDmaHandle(void);

/**
  * @brief  Read VREFINT + TEMPSENSOR in a single DMA-pause window and
  *         return live VDDA (mV) and die-temperature (degC) via out-params.
  *         This is the preferred entry point for periodic diagnostics --
  *         it avoids pausing the streaming pipeline twice per update and
  *         keeps the ADC factory calibration (ADCAL) intact across the
  *         trigger switch.  Caller must follow up with adcStartFrame().
  * @param  vddaMvOut    Out: VDDA in mV (VDDA_FALLBACK_MV on failure)
  * @param  dieTempCOut  Out: Die temperature degC (-128 on failure)
  * @retval None
  */
void adcReadDiagnostics(uint32_t *vddaMvOut, int16_t *dieTempCOut);

/**
  * @brief  Convenience wrapper that returns only VDDA.  Internally calls
  *         adcReadDiagnostics(); the die-temp result is discarded.
  *         Caller must follow up with adcStartFrame().
  * @retval VDDA in mV.  Returns 3300 on failure.
  */
uint32_t adcReadVddaMv(void);

/**
  * @brief  Convenience wrapper that returns only die-temperature.
  *         Internally calls adcReadDiagnostics().
  *         Caller must follow up with adcStartFrame().
  * @retval Die temperature in degC.  Returns -128 on failure.
  */
int16_t adcReadDieTempC(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

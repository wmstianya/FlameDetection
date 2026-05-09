/**
  ******************************************************************************
  * @file    flame.c
  * @brief   Industrial flame detection: trimmed-mean filter, fault-detection
  *          gate, adaptive baseline tracking with Schmitt-trigger thresholds,
  *          and a state machine driving relay (PA11) and LED (PA10).
  *
  *          The fault gate runs BEFORE the state machine and forces
  *          relay = OFF whenever any safety judgement triggers, regardless
  *          of what the state machine would otherwise decide.  This protects
  *          against the wet-probe / open-circuit / non-physical-signal
  *          failure modes typical of industrial boiler-room deployments.
  *
 * @author  2026-05-09
 * @date    2026-05-09
 * @version V1.3.0
 *
 * @revision
 *   V1.0.0  2026-05-08  Initial: DMA sampling + fault gate + auto-recover
 *   V1.1.0  2026-05-09  Post-J-Link-probe hardening: relative SHORT_DROP_MV
 *                        check catches wet shorts above FAULT_FLOOR_MV;
 *                        FAULT is now sticky; recoverCounter removed.
 *   V1.2.0  2026-05-09  Review fixes:
 *                        - new Err6 (FAULT_CODE_SHORT_REL) for wet shorts
 *                          (keeps Err1 meaning dead-short for maintenance)
 *                        - isRelativeShort() extracted; evaluateFaultGate
 *                          back under 20 lines
 *                        - handleStateFault() removed, maybeEnterFault()
 *                          returns 1 directly when already in FAULT
 *                        - accumulateBaseline() rejects out-of-window
 *                          samples so a boot-time short cannot seed
 *                          baseline across a reset (reset-cycle lock-out,
 *                          NOT UL-296 manual-reset: that needs VBAT +
 *                          BKP which this board lacks)
 *   V1.3.0  2026-05-09  Ion-probe field-hardening:
 *                        - 50 Hz IIR notch biquad (Q15, fs=1kHz, Q=5)
 *                          removes mains EMI from high-Z probe
 *                        - Ignition-spark frame discard: clip detection
 *                          drops frames with > 16 saturated samples
 *                        - Baseline drift monitor (Err7 DRIFT): detects
 *                          gradual soot/moisture leakage on the probe
  ******************************************************************************
  */
#include "flame.h"
#include "adc.h"
#include "tm1650.h"
#include <string.h>             /* memcpy for DMA frame copy */

/* -------------------------------------------------------------------------- */
/*                       Internal constants                                   */
/* -------------------------------------------------------------------------- */
#define LED_BLINK_HALF_PERIOD_MS    250U  /* 2 Hz blink in FAULT state       */
#define DIE_TEMP_UPDATE_PERIOD      8U    /* refresh die-temp every N cycles */

/* -------------------------------------------------------------------------- */
/*                       Module state                                         */
/* -------------------------------------------------------------------------- */

/* Sample buffer (BSS, 256 bytes) shared by collectSamples + computeStats */
static uint16_t  sampleBuffer[ADC_SAMPLE_COUNT];

/* State and diagnostics -- written by main loop, read everywhere */
static volatile flameState_t flameState         = FLAME_STATE_BLANKING;
static          uint16_t     lastAdcMv          = 0U;
static          uint16_t     baselineMv         = 3000U;
static          uint8_t      faultCode          = FAULT_CODE_NONE;
static          int16_t      dieTempC           = 0;

/* Counters owned by main loop (no ISR write) */
static uint8_t   dvFaultCounter      = 0U;
static uint8_t   baselineSampleIdx   = 0U;
static uint32_t  baselineAccumulator = 0U;
static uint8_t   diagRefreshCounter  = 0U;
static uint32_t  cachedVddaMv        = 3300U;   /* refreshed every VDDA_REFRESH_PERIOD frames */

/* Baseline drift monitoring (probe soot / moisture detection) */
static uint16_t  baselineAtEntry     = 3000U;   /* baseline snapshot at NO_FLAME entry */
static uint16_t  driftCheckCounter   = 0U;

/* Counters incremented in SysTick ISR (volatile) */
static volatile uint16_t flameOffCount   = 0U;  /* ms since hi-threshold cross */
static volatile uint16_t firstStartCount = 0U;  /* ms since boot, capped       */
static volatile uint16_t blinkCounterMs  = 0U;  /* drives LED blink in FAULT   */

/* Statistics returned by computeStatistics() */
typedef struct
{
    uint16_t meanRaw;       /* trimmed mean (12-bit ADC counts)  */
    uint16_t varianceRaw;   /* sample variance, ADC-counts squared (clipped) */
} adcStats_t;

/* -------------------------------------------------------------------------- */
/*                       Sampling                                             */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Wait for the ADC DMA to complete a 128-sample frame, copy into
  *         the local buffer, and arm the next frame so it overlaps with
  *         processing.  CPU is asleep (__WFI) for ~127.5 ms of every 128 ms
  *         frame; DMA TC interrupt or SysTick wakes us.
  * @retval None
  */
static void collectSamples(void)
{
    while (adcFrameReady() == 0U)
    {
        __WFI();
    }
    memcpy(sampleBuffer, adcGetBuffer(), sizeof(sampleBuffer));
    adcClearFrameFlag();
    adcStartFrame();    /* fire next frame so DMA runs while we process */
}

/* -------------------------------------------------------------------------- */
/*                       Pre-processing: ignition & 50 Hz notch               */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Detect an ignition-spark frame by counting ADC samples that are
  *         clipped to the rail or ground.  A high-voltage ignition pulse
  *         saturates the front-end amplifier, producing clusters of 0 or
  *         4095 readings.  If more than CLIP_DISCARD_THRESHOLD samples are
  *         clipped, the frame is declared invalid and should be discarded.
  * @retval 1 if the frame is an ignition frame (discard), 0 if normal
  */
static uint8_t isIgnitionFrame(void)
{
    uint16_t clipCount = 0U;
    for (uint16_t i = 0U; i < ADC_SAMPLE_COUNT; i++)
    {
        if ((sampleBuffer[i] < ADC_CLIP_LOW) ||
            (sampleBuffer[i] > ADC_CLIP_HIGH))
        {
            clipCount++;
        }
    }
    return (clipCount > CLIP_DISCARD_THRESHOLD) ? 1U : 0U;
}

/**
  * @brief  In-place 50 Hz IIR notch filter (second-order biquad).
  *
  *         Removes mains EMI picked up by the high-impedance ion probe.
  *         The notch is narrow (Q=5, BW ~10 Hz) so the 1~15 Hz flame-
  *         flicker band passes through unaffected.
  *
  *         Coefficients (float reference, fs=1000, f0=50, Q=5):
  *           w0 = 2*pi*50/1000 = 0.31416
  *           alpha = sin(w0)/(2*Q) = 0.030902
  *           b0 =  1.0           b1 = -1.90211   b2 =  1.0
  *           a0 =  1.030902      a1 = -1.90211   a2 =  0.969098
  *         Normalised (divide by a0):
  *           b0n = 0.97000   b1n = -1.84434   b2n = 0.97000
  *           a1n = -1.84434  a2n =  0.94000
  *
  *         Implemented in Q15 fixed-point (scale = 32768):
  *           B0 = 31785   B1 = -60424   B2 = 31785
  *           A1 = -60424  A2 =  30802
  *
  *         Direct Form II Transposed, 32-bit accumulator.
  *         State is reset at the start of each frame (no inter-frame
  *         memory) because the 128 ms gap would make the filter ring
  *         on the first few samples of the next frame otherwise.
  *
  * @param  buf  Sample buffer (modified in place)
  * @param  n    Number of samples
  * @retval None
  */
static void applyNotch50Hz(uint16_t *buf, uint16_t n)
{
    /* Q15 coefficients */
    static const int32_t B0 =  31785;
    static const int32_t B1 = -60424;
    static const int32_t B2 =  31785;
    static const int32_t A1 = -60424;
    static const int32_t A2 =  30802;

    int32_t z1 = 0;
    int32_t z2 = 0;

    for (uint16_t i = 0U; i < n; i++)
    {
        int32_t x = (int32_t)buf[i];
        int32_t y = (B0 * x + z1) >> 15;
        z1 = B1 * x - A1 * y + z2;
        z2 = B2 * x - A2 * y;
        /* Clamp to valid ADC range */
        if (y < 0)    { y = 0; }
        if (y > 4095) { y = 4095; }
        buf[i] = (uint16_t)y;
    }
}

/**
  * @brief  In-place ascending insertion sort.  N is small (128), Cortex-M0+
  *         executes this in ~250 us which is negligible vs the 128 ms cycle.
  * @param  buf  Pointer to uint16_t buffer
  * @param  n    Number of elements
  * @retval None
  */
static void sortAscending(uint16_t *buf, uint16_t n)
{
    for (uint16_t i = 1U; i < n; i++)
    {
        uint16_t key = buf[i];
        uint16_t j   = i;
        while ((j > 0U) && (buf[j - 1U] > key))
        {
            buf[j] = buf[j - 1U];
            j--;
        }
        buf[j] = key;
    }
}

/**
  * @brief  Compute trimmed mean of the middle [TRIM_BOTTOM .. N-TRIM_TOP-1]
  *         samples after sorting in place.
  * @retval Trimmed mean as 12-bit ADC counts
  */
static uint16_t computeTrimmedMean(void)
{
    uint32_t sum = 0U;
    for (uint16_t i = TRIM_BOTTOM_COUNT;
         i < (ADC_SAMPLE_COUNT - TRIM_TOP_COUNT);
         i++)
    {
        sum += sampleBuffer[i];
    }
    return (uint16_t)(sum / TRIMMED_SAMPLE_COUNT);
}

/**
  * @brief  Compute sample variance over the trimmed middle section.
  *         Real flame flicker yields variance >> MIN_FLAME_VARIANCE; a
  *         shorted/wet probe yields near-zero variance.
  * @param  mean  Trimmed mean returned by computeTrimmedMean()
  * @retval Variance (clipped to UINT16_MAX)
  */
static uint16_t computeTrimmedVariance(uint16_t mean)
{
    uint32_t sumSq = 0U;
    for (uint16_t i = TRIM_BOTTOM_COUNT;
         i < (ADC_SAMPLE_COUNT - TRIM_TOP_COUNT);
         i++)
    {
        int32_t dev = (int32_t)sampleBuffer[i] - (int32_t)mean;
        sumSq += (uint32_t)(dev * dev);
    }
    uint32_t var = sumSq / TRIMMED_SAMPLE_COUNT;
    return (var > 0xFFFFU) ? 0xFFFFU : (uint16_t)var;
}

/**
  * @brief  Sort sampleBuffer in place and compute trimmed mean + variance.
  * @retval Statistics struct
  */
static adcStats_t computeStatistics(void)
{
    adcStats_t s;
    sortAscending(sampleBuffer, ADC_SAMPLE_COUNT);
    s.meanRaw     = computeTrimmedMean();
    s.varianceRaw = computeTrimmedVariance(s.meanRaw);
    return s;
}

/**
  * @brief  Convert raw ADC counts to mV using current VDDA.
  * @param  raw   12-bit ADC counts
  * @param  vdda  Live VDDA in mV (from adcReadVddaMv())
  * @retval mV (saturating at UINT16_MAX, which never happens in practice)
  */
static uint16_t adcRawToMv(uint16_t raw, uint32_t vdda)
{
    uint32_t mv = ((uint32_t)raw * vdda) / ADC_FULL_SCALE;
    return (mv > 0xFFFFU) ? 0xFFFFU : (uint16_t)mv;
}

/* -------------------------------------------------------------------------- */
/*                       Fault detection gate                                 */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Check whether the current sample looks like a wet/partial short:
  *         signal collapsed well below the live baseline AND variance died.
  *         Gated on "baseline is trusted" so BLANKING / WAIT_BASELINE / FAULT
  *         cannot spuriously trip it.
  * @param  mv        Current mV reading
  * @param  variance  Sample variance over trimmed window
  * @param  state     Current flame state
  * @retval 1 if relative-short pattern matches, 0 otherwise
  */
static uint8_t isRelativeShort(uint16_t     mv,
                               uint16_t     variance,
                               flameState_t state)
{
    uint8_t baselineTrusted = (state == FLAME_STATE_NO_FLAME)
                              || (state == FLAME_STATE_FLAME_ON)
                              || (state == FLAME_STATE_CONFIRM_OFF);
    if (!baselineTrusted)               { return 0U; }
    if (baselineMv <= SHORT_DROP_MV)    { return 0U; }
    if (mv >= (uint16_t)(baselineMv - SHORT_DROP_MV)) { return 0U; }
    return (variance < MIN_FLAME_VARIANCE) ? 1U : 0U;
}

/**
  * @brief  Check for slow baseline drift caused by probe soot or moisture.
  *         Called every frame while in NO_FLAME.  If the baseline has drifted
  *         down by more than BASELINE_DRIFT_WARN_MV since entering NO_FLAME,
  *         the probe needs maintenance.
  * @retval FAULT_CODE_DRIFT if drift exceeded, FAULT_CODE_NONE otherwise
  */
static uint8_t checkBaselineDrift(void)
{
    if (flameState != FLAME_STATE_NO_FLAME) { return FAULT_CODE_NONE; }
    driftCheckCounter++;
    if (driftCheckCounter < DRIFT_CHECK_INTERVAL)  { return FAULT_CODE_NONE; }
    driftCheckCounter = 0U;
    if ((baselineAtEntry > baselineMv) &&
        ((baselineAtEntry - baselineMv) > BASELINE_DRIFT_WARN_MV))
    {
        return FAULT_CODE_DRIFT;
    }
    return FAULT_CODE_NONE;
}

/**
  * @brief  Decide whether the current cycle should trigger a fault.
  *         Order matters: most-specific failure modes checked first so the
  *         operator sees the most actionable Err code.
  * @param  mv        Current trimmed-mean reading in mV
  * @param  variance  Sample variance over trimmed window
  * @param  vdda      Live VDDA in mV
  * @param  state     Current flame state
  * @retval Fault code (0 = no fault)
  */
static uint8_t evaluateFaultGate(uint16_t mv,
                                 uint16_t variance,
                                 uint32_t vdda,
                                 flameState_t state)
{
    if (mv < FAULT_FLOOR_MV)    { return FAULT_CODE_FLOOR;   }
    if (mv > FAULT_CEILING_MV)  { return FAULT_CODE_CEILING; }
    if ((vdda < VDDA_MIN_MV) || (vdda > VDDA_MAX_MV))
    {
        return FAULT_CODE_VDDA;
    }
    if (isRelativeShort(mv, variance, state)) { return FAULT_CODE_SHORT_REL; }
    if (((state == FLAME_STATE_FLAME_ON) || (state == FLAME_STATE_CONFIRM_OFF))
        && (variance < MIN_FLAME_VARIANCE))
    {
        return FAULT_CODE_VARIANCE;
    }
    return FAULT_CODE_NONE;
}

/**
  * @brief  Track non-physical jumps in the signal.  Called on every cycle
  *         except the very first, while in normal operating states.
  * @param  mv  Current mV reading
  * @retval 1 if dv-fault confirmed, 0 otherwise
  */
static uint8_t checkJumpFault(uint16_t mv)
{
    uint16_t dv = (mv > lastAdcMv) ? (mv - lastAdcMv) : (lastAdcMv - mv);
    if (dv > MAX_DV_PER_CYCLE_MV)
    {
        dvFaultCounter++;
        if (dvFaultCounter >= DV_FAULT_CONFIRM_COUNT)
        {
            return 1U;
        }
    }
    else
    {
        dvFaultCounter = 0U;
    }
    return 0U;
}

/* -------------------------------------------------------------------------- */
/*                       Baseline                                             */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Slow EMA of baseline -- only run while NO_FLAME confirmed.
  *         tau ~ 32 cycles ~ 4 s.
  * @param  mv  Current mV reading
  * @retval None
  */
static void updateBaselineEma(uint16_t mv)
{
    uint32_t b = baselineMv;
    b = b - (b >> BASELINE_EMA_SHIFT) + ((uint32_t)mv >> BASELINE_EMA_SHIFT);
    baselineMv = (uint16_t)b;
}

/**
  * @brief  Reset baseline-capture accumulators when entering WAIT_BASELINE.
  * @retval None
  */
static void resetBaselineCapture(void)
{
    baselineSampleIdx   = 0U;
    baselineAccumulator = 0U;
}

/**
  * @brief  Accumulate one sample into baseline averaging during
  *         WAIT_BASELINE.  Samples physically implausible at power-up
  *         (below BASELINE_SANE_MIN_MV or above BASELINE_SANE_MAX_MV) are
  *         rejected -- a stuck input (wet probe / shorted wiring / open
  *         sensor present at boot) will therefore never become a trusted
  *         baseline.  The state machine stalls in WAIT_BASELINE and IWDG
  *         eventually resets the MCU.  Operator intervention (inspect and
  *         rectify the probe) is required to leave the reset loop.
  * @param  mv  Current mV reading
  * @retval 1 if baseline captured this cycle, 0 if still collecting
  */
static uint8_t accumulateBaseline(uint16_t mv)
{
    if ((mv < BASELINE_SANE_MIN_MV) || (mv > BASELINE_SANE_MAX_MV))
    {
        return 0U;
    }
    baselineAccumulator += mv;
    baselineSampleIdx++;
    if (baselineSampleIdx >= BASELINE_INIT_SAMPLES)
    {
        baselineMv = (uint16_t)(baselineAccumulator / BASELINE_INIT_SAMPLES);
        return 1U;
    }
    return 0U;
}

/* -------------------------------------------------------------------------- */
/*                       State transitions                                    */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Centralised state setter -- clears state-local counters so each
  *         state starts fresh on entry.
  * @param  newState  Target state
  * @retval None
  */
static void changeState(flameState_t newState)
{
    flameState     = newState;
    flameOffCount  = 0U;
    blinkCounterMs = 0U;
    if (newState == FLAME_STATE_WAIT_BASELINE)
    {
        resetBaselineCapture();
    }
    if (newState == FLAME_STATE_NO_FLAME)
    {
        baselineAtEntry    = baselineMv;
        driftCheckCounter  = 0U;
    }
}

/**
  * @brief  BLANKING -- ignore all readings for the first STARTUP_DELAY_MS,
  *         then move on to baseline capture.
  * @retval None
  */
static void handleStateBlanking(void)
{
    if (firstStartCount >= STARTUP_DELAY_MS)
    {
        changeState(FLAME_STATE_WAIT_BASELINE);
    }
}

/**
  * @brief  WAIT_BASELINE -- average BASELINE_INIT_SAMPLES plausible reads
  *         to seed the baseline, then move to NO_FLAME.
  * @param  mv  Current mV reading
  * @retval None
  */
static void handleStateWaitBaseline(uint16_t mv)
{
    if (accumulateBaseline(mv))
    {
        changeState(FLAME_STATE_NO_FLAME);
    }
}

/**
  * @brief  NO_FLAME -- slow-update baseline; declare FLAME_ON if signal
  *         drops by >= ON_DELTA AND has flame-typical variance.
  * @param  mv        Current mV reading
  * @param  variance  Sample variance
  * @retval None
  */
static void handleStateNoFlame(uint16_t mv, uint16_t variance)
{
    updateBaselineEma(mv);

    uint16_t onThreshold = (baselineMv > FLAME_ON_DELTA_MV)
                          ? (uint16_t)(baselineMv - FLAME_ON_DELTA_MV)
                          : 0U;

    if ((mv < onThreshold) && (variance >= MIN_FLAME_VARIANCE))
    {
        changeState(FLAME_STATE_FLAME_ON);
    }
}

/**
  * @brief  FLAME_ON -- if signal climbs above OFF threshold, enter
  *         CONFIRM_OFF and start the de-bounce timer.
  * @param  mv  Current mV reading
  * @retval None
  */
static void handleStateFlameOn(uint16_t mv)
{
    uint16_t offThreshold = (baselineMv > FLAME_OFF_DELTA_MV)
                           ? (uint16_t)(baselineMv - FLAME_OFF_DELTA_MV)
                           : 0U;

    if (mv >= offThreshold)
    {
        changeState(FLAME_STATE_CONFIRM_OFF);
    }
}

/**
  * @brief  CONFIRM_OFF -- if signal drops back below ON_DELTA, return to
  *         FLAME_ON.  If above OFF_DELTA for FLAME_OFF_TIMEOUT_MS, declare
  *         flame extinguished.
  * @param  mv  Current mV reading
  * @retval None
  */
static void handleStateConfirmOff(uint16_t mv)
{
    uint16_t onThreshold  = (baselineMv > FLAME_ON_DELTA_MV)
                           ? (uint16_t)(baselineMv - FLAME_ON_DELTA_MV)
                           : 0U;

    if (mv < onThreshold)
    {
        changeState(FLAME_STATE_FLAME_ON);
        return;
    }
    if (flameOffCount >= FLAME_OFF_TIMEOUT_MS)
    {
        changeState(FLAME_STATE_NO_FLAME);
    }
}

/* FAULT is sticky: once entered, the state machine cannot self-clear.
 * Handled inline in maybeEnterFault() -- no dedicated handler needed. */

/* -------------------------------------------------------------------------- */
/*                       Output drivers                                       */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Drive relay + LED + display based on the current state.
  *         FAULT enforces relay = OFF (fail-safe).  LED in FAULT is driven
  *         by the SysTick blink so we leave it untouched here.
  * @param  mv  Current mV reading (for normal display)
  * @retval None
  */
static void driveOutputs(uint16_t mv)
{
    flameState_t s = flameState;

    if (s == FLAME_STATE_FAULT)
    {
        HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
        tm1650ShowFaultCode(faultCode);
        return;
    }
    if (s == FLAME_STATE_FLAME_ON)
    {
        HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_PORT,   LED_PIN,   GPIO_PIN_RESET); /* LED ON  */
    }
    else
    {
        HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_PORT,   LED_PIN,   GPIO_PIN_SET);   /* LED OFF */
    }
    tm1650ShowValue(mv);
}

/* -------------------------------------------------------------------------- */
/*                       Public API                                           */
/* -------------------------------------------------------------------------- */

void flameInit(void)
{
    flameState          = FLAME_STATE_BLANKING;
    lastAdcMv           = 0U;
    baselineMv          = 3000U;
    faultCode           = FAULT_CODE_NONE;
    dieTempC            = 0;
    dvFaultCounter      = 0U;
    diagRefreshCounter  = 0U;
    cachedVddaMv        = 3300U;
    baselineAtEntry     = 3000U;
    driftCheckCounter   = 0U;
    resetBaselineCapture();

    flameOffCount   = 0U;
    firstStartCount = 0U;
    blinkCounterMs  = 0U;
}

/**
  * @brief  Refresh cached VDDA and die-temperature diagnostic at low rate
  *         (every VDDA_REFRESH_PERIOD frames ~= 1 s).  Both internal
  *         channels are read inside a single DMA-pause window via
  *         adcReadDiagnostics(), then the streaming frame is re-armed.
  *         Consolidated from the separate vdda/dietemp refresh helpers to
  *         eliminate the risk of double-starting DMA on the same cycle.
  * @retval None
  */
static void refreshDiagnosticsIfDue(void)
{
    diagRefreshCounter++;
    if (diagRefreshCounter >= VDDA_REFRESH_PERIOD)
    {
        diagRefreshCounter = 0U;
        adcReadDiagnostics(&cachedVddaMv, &dieTempC);
        adcStartFrame();
    }
}

/**
  * @brief  Run normal (non-FAULT) state-machine dispatch.
  * @param  mv        Current mV reading
  * @param  variance  Sample variance (gates flame entry)
  * @retval None
  */
static void dispatchNormalState(uint16_t mv, uint16_t variance)
{
    switch (flameState)
    {
        case FLAME_STATE_BLANKING:      handleStateBlanking();             break;
        case FLAME_STATE_WAIT_BASELINE: handleStateWaitBaseline(mv);       break;
        case FLAME_STATE_NO_FLAME:      handleStateNoFlame(mv, variance);  break;
        case FLAME_STATE_FLAME_ON:      handleStateFlameOn(mv);            break;
        case FLAME_STATE_CONFIRM_OFF:   handleStateConfirmOff(mv);         break;
        default:                                                            break;
    }
}

/**
  * @brief  Try to enter FAULT.  Once FAULT is set, this function returns
  *         1 and performs no further transitions: the state is sticky
  *         until the next reset.  flameInit()'s BASELINE_SANE guard then
  *         prevents a persistent short from re-seeding the baseline.
  * @param  newFault  Fault code from evaluateFaultGate (0 = no fault)
  * @retval 1 if FAULT is active (entered this cycle or latched), 0 otherwise
  */
static uint8_t maybeEnterFault(uint8_t newFault)
{
    if (flameState == FLAME_STATE_FAULT)
    {
        return 1U;
    }
    if (newFault != FAULT_CODE_NONE)
    {
        faultCode = newFault;
        changeState(FLAME_STATE_FAULT);
        return 1U;
    }
    return 0U;
}

void flameProcess(void)
{
    collectSamples();

    /* --- Pre-processing: discard ignition frames, remove 50 Hz EMI --- */
    if (isIgnitionFrame())
    {
        return;  /* IWDG is fed in main loop, safe to skip this frame */
    }
    applyNotch50Hz(sampleBuffer, ADC_SAMPLE_COUNT);

    adcStats_t stats = computeStatistics();

    uint16_t mv = adcRawToMv(stats.meanRaw, cachedVddaMv);

    /* Jump fault is its own check (can override the gate's verdict) */
    uint8_t newFault = evaluateFaultGate(mv,
                                         stats.varianceRaw,
                                         cachedVddaMv,
                                         flameState);
    if ((newFault == FAULT_CODE_NONE) && (lastAdcMv != 0U))
    {
        if (checkJumpFault(mv))
        {
            newFault = FAULT_CODE_JUMP;
        }
    }

    if (maybeEnterFault(newFault) == 0U)
    {
        dispatchNormalState(mv, stats.varianceRaw);

        /* Baseline drift check (only active in NO_FLAME) */
        uint8_t driftFault = checkBaselineDrift();
        if (driftFault != FAULT_CODE_NONE)
        {
            (void)maybeEnterFault(driftFault);
        }
    }

    lastAdcMv = mv;
    driveOutputs(mv);

    /* Low-rate diagnostic refresh: single DMA-pause window reads both
       VREFINT and TEMPSENSOR, then re-arms the streaming frame. */
    refreshDiagnosticsIfDue();
}

void flameTickCallback(void)
{
    if (flameOffCount <= FLAME_OFF_TIMEOUT_MS)
    {
        flameOffCount++;
    }
    if (firstStartCount < STARTUP_DELAY_MS)
    {
        firstStartCount++;
    }

    /* 2 Hz LED blink while in FAULT.  Other states drive LED from main. */
    if (flameState == FLAME_STATE_FAULT)
    {
        blinkCounterMs++;
        if (blinkCounterMs >= LED_BLINK_HALF_PERIOD_MS)
        {
            blinkCounterMs = 0U;
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        }
    }
}

flameState_t flameGetState(void)       { return flameState;  }
uint16_t     flameGetLastMv(void)      { return lastAdcMv;   }
uint16_t     flameGetBaselineMv(void)  { return baselineMv;  }
uint8_t      flameGetFaultCode(void)   { return faultCode;   }
int16_t      flameGetDieTempC(void)    { return dieTempC;    }

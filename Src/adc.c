/**
  ******************************************************************************
  * @file    adc.c
  * @brief   Non-blocking ADC driver for STM32G070: TIM6 1 kHz triggers ADC1,
  *          results pushed to sampleBuffer[128] via DMA1 channel 1.  4x HW
  *          oversampling acts as a pre-filter; the application stacks a
  *          software trimmed-mean on top.
  *
  *          VREFINT / TEMPSENSOR are read via a Stop_DMA -> ConfigChannel
  *          (internal) -> polling -> ConfigChannel(CH1) sequence.  The
  *          application is responsible for calling adcStartFrame() afterward
  *          to resume the streaming pipeline.
  *
  * @author  2026-05-08
  * @date    2026-05-08
  * @version V1.3.0
  ******************************************************************************
  */
#include "adc.h"

/* ---- Pin / timing configuration ------------------------------------------ */
#define ADC_POLL_TIMEOUT_MS     10U
#define ADC_SENSOR_PIN          GPIO_PIN_1
#define ADC_SENSOR_PORT         GPIOA

/* ---- VDDA / die-temp factory calibration --------------------------------- */
#define VDDA_FALLBACK_MV        3300U
#define DIETEMP_FALLBACK_C      ((int16_t)-128)

/* ---- TIM6 configuration -------------------------------------------------- *
 * SYSCLK = 64 MHz.  Pre = 64-1 -> CK_CNT = 1 MHz.  Period = 1000-1 -> overflow
 * every 1 ms -> TRGO fires at 1 kHz, driving ADC.
 */
#define TIM6_PRESCALER          (64U - 1U)
#define TIM6_PERIOD             (1000U - 1U)

/* ---- IRQ priorities (Cortex-M0+: 0=highest, 3=lowest) -------------------- */
#define DMA_ADC_IRQ_PRIORITY    1U

/* ---- ADC trigger-source CFGR1 values (EXTEN[11:10] + EXTSEL[8:6]) -------- *
 *
 * Rather than re-invoking HAL_ADC_Init() to swap triggers (which performs a
 * full ADC disable/reset and destroys the factory calibration coefficients),
 * we toggle the CFGR1 fields directly.  This preserves ADCAL across
 * VREFINT / TEMPSENSOR reads.
 *
 * Software trigger: EXTEN = 00, EXTSEL = 000  -> CFGR1[11:6] = 0
 * TIM6 TRGO, rising: EXTEN = 01, EXTSEL = 101 -> (1<<10) | (5<<6) = 0x540
 */
#define ADC_TRIG_MASK           (ADC_CFGR1_EXTEN_Msk | ADC_CFGR1_EXTSEL_Msk)
#define ADC_TRIG_SOFTWARE_VAL   0U
#define ADC_TRIG_T6_RISING_VAL  ((1UL << ADC_CFGR1_EXTEN_Pos) \
                                 | (5UL << ADC_CFGR1_EXTSEL_Pos))

static ADC_HandleTypeDef hadc;
static DMA_HandleTypeDef hdmaAdc;
static TIM_HandleTypeDef htim6;

/* DMA target buffer.  uint16_t matches ADC 12-bit right-aligned output. */
static uint16_t           sampleBuffer[ADC_SAMPLE_COUNT];
static volatile uint8_t   frameReady = 0U;

/* -------------------------------------------------------------------------- */
/*                       Init helpers (split for <=20-line rule)              */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Configure 4x ADC oversampling parameters.
  * @param  init  Pointer to ADC_InitTypeDef to populate
  * @retval None
  */
static void adcConfigureOversampling(ADC_InitTypeDef *init)
{
    init->OversamplingMode               = ENABLE;
    init->Oversampling.Ratio             = ADC_OVERSAMPLING_RATIO_4;
    init->Oversampling.RightBitShift     = ADC_RIGHTBITSHIFT_2;
    init->Oversampling.TriggeredMode     = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
}

/**
  * @brief  Populate ADC core init for TIM6-triggered DMA streaming on CH1.
  * @param  init  Pointer to ADC_InitTypeDef to populate
  * @retval None
  */
static void adcCoreInit(ADC_InitTypeDef *init)
{
    init->ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
    init->Resolution            = ADC_RESOLUTION_12B;
    init->DataAlign             = ADC_DATAALIGN_RIGHT;
    init->ScanConvMode          = ADC_SCAN_DISABLE;
    init->EOCSelection          = ADC_EOC_SINGLE_CONV;
    init->LowPowerAutoWait      = DISABLE;
    init->LowPowerAutoPowerOff  = DISABLE;
    init->ContinuousConvMode    = DISABLE;
    init->DiscontinuousConvMode = DISABLE;
    init->NbrOfConversion       = 1U;
    init->ExternalTrigConv      = ADC_EXTERNALTRIG_T6_TRGO;
    init->ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    init->DMAContinuousRequests = ENABLE;
    init->Overrun               = ADC_OVR_DATA_PRESERVED;
    init->SamplingTimeCommon1   = ADC_SAMPLETIME_160CYCLES_5;
    init->SamplingTimeCommon2   = ADC_SAMPLETIME_160CYCLES_5;
    init->TriggerFrequencyMode  = ADC_TRIGGER_FREQ_HIGH;
    adcConfigureOversampling(init);
}

/**
  * @brief  Apply CH1 (PA1 flame sensor) as the regular-rank-1 channel.
  * @retval None
  */
static void adcSelectFlameChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = ADC_CHANNEL_1;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief  Initialise TIM6 to issue a TRGO update event at 1 kHz.
  * @retval None
  */
static void MX_TIM6_Init(void)
{
    TIM_MasterConfigTypeDef sMasterCfg = {0};

    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = TIM6_PRESCALER;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = TIM6_PERIOD;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    sMasterCfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterCfg.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterCfg) != HAL_OK)
    {
        Error_Handler();
    }
}

/* -------------------------------------------------------------------------- */
/*                       Public init                                          */
/* -------------------------------------------------------------------------- */

void MX_ADC1_Init(void)
{
    hadc.Instance = ADC1;
    adcCoreInit(&hadc.Init);
    if (HAL_ADC_Init(&hadc) != HAL_OK)
    {
        Error_Handler();
    }
    adcSelectFlameChannel();
    if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK)
    {
        Error_Handler();
    }
    MX_TIM6_Init();
    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
    {
        Error_Handler();
    }
}

/* -------------------------------------------------------------------------- */
/*                       Frame streaming API                                  */
/* -------------------------------------------------------------------------- */

void adcStartFrame(void)
{
    frameReady = 0U;
    if (HAL_ADC_Start_DMA(&hadc,
                          (uint32_t *)sampleBuffer,
                          ADC_SAMPLE_COUNT) != HAL_OK)
    {
        Error_Handler();
    }
}

uint8_t adcFrameReady(void)
{
    return frameReady;
}

const uint16_t *adcGetBuffer(void)
{
    return sampleBuffer;
}

void adcClearFrameFlag(void)
{
    frameReady = 0U;
}

DMA_HandleTypeDef *adcGetDmaHandle(void)
{
    return &hdmaAdc;
}

/**
  * @brief  HAL DMA transfer-complete callback (called from
  *         HAL_DMA_IRQHandler -> ADC layer).  Sets the frame-ready flag
  *         and stops the DMA so the buffer is stable for processing.
  * @param  hadcParam  ADC handle (unused, single instance)
  * @retval None
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadcParam)
{
    (void)hadcParam;
    (void)HAL_ADC_Stop_DMA(&hadc);
    frameReady = 1U;
}

/* -------------------------------------------------------------------------- */
/*                       Internal-channel polled reads                        */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Apply the given CFGR1 EXTEN/EXTSEL bits without disturbing the
  *         rest of the ADC configuration.  Safe to call while ADC is idle.
  *         ADC must not be actively converting when CFGR1 is changed
  *         (RM0454 14.4.6).
  * @param  trigBits  Either ADC_TRIG_SOFTWARE_VAL or ADC_TRIG_T6_RISING_VAL
  * @retval None
  */
static void adcSetTriggerBits(uint32_t trigBits)
{
    MODIFY_REG(ADC1->CFGR1, ADC_TRIG_MASK, trigBits);
}

/**
  * @brief  Issue one polled conversion using the currently configured
  *         channel.  Used for VREFINT/TEMPSENSOR after Stop_DMA.
  * @retval 12-bit result, 0 on HAL failure
  */
static uint32_t adcOnePolledConversion(void)
{
    if (HAL_ADC_Start(&hadc) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc, ADC_POLL_TIMEOUT_MS) != HAL_OK)
    {
        return 0U;
    }
    return HAL_ADC_GetValue(&hadc);
}

/**
  * @brief  Select a channel for a one-shot polled read, performing an
  *         initial discard conversion for internal channels to give the
  *         bandgap / sensor cell time to stabilise.  Caller must have
  *         already switched the trigger to software mode.
  * @param  channel  HAL ADC_CHANNEL_*
  * @retval 12-bit result
  */
static uint32_t adcPolledReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};

    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc, &cfg) != HAL_OK) { return 0U; }

    (void)adcOnePolledConversion();   /* discard first sample */
    return adcOnePolledConversion();
}

/**
  * @brief  Read VREFINT + TEMPSENSOR atomically and return VDDA (mV) and
  *         die-temperature (degC) via out-params.
  *
  *         Protocol inside a single Stop_DMA window:
  *           1) Stop active DMA frame (if any)
  *           2) Switch trigger to SOFTWARE via MODIFY_REG (keeps ADCAL!)
  *           3) Polled read VREFINT (dummy + real)
  *           4) Polled read TEMPSENSOR (dummy + real)
  *           5) Restore trigger to T6_TRGO
  *           6) Re-select flame sensor channel (CH1)
  *
  *         Caller must call adcStartFrame() afterwards to resume streaming.
  *
  * @param  vddaMvOut    Out: VDDA in mV (VDDA_FALLBACK_MV on failure)
  * @param  dieTempCOut  Out: Die temperature in degC (DIETEMP_FALLBACK_C on failure)
  * @retval None
  */
void adcReadDiagnostics(uint32_t *vddaMvOut, int16_t *dieTempCOut)
{
    uint32_t vrefCal = (uint32_t)(*VREFINT_CAL_ADDR);
    int32_t  tsCal1  = (int32_t)(*TEMPSENSOR_CAL1_ADDR);
    int32_t  tsCal2  = (int32_t)(*TEMPSENSOR_CAL2_ADDR);

    (void)HAL_ADC_Stop_DMA(&hadc);
    adcSetTriggerBits(ADC_TRIG_SOFTWARE_VAL);

    uint32_t vrefNow = adcPolledReadChannel(ADC_CHANNEL_VREFINT);
    uint32_t tsNow   = adcPolledReadChannel(ADC_CHANNEL_TEMPSENSOR);

    adcSetTriggerBits(ADC_TRIG_T6_RISING_VAL);
    adcSelectFlameChannel();

    /* VDDA from VREFINT */
    if ((vrefNow == 0U) || (vrefCal == 0U))
    {
        *vddaMvOut = VDDA_FALLBACK_MV;
    }
    else
    {
        *vddaMvOut = (VREFINT_CAL_VREF * vrefCal) / vrefNow;
    }

    /* Die temp from TEMPSENSOR + live VDDA */
    if ((tsNow == 0U) || (tsCal1 == tsCal2))
    {
        *dieTempCOut = DIETEMP_FALLBACK_C;
        return;
    }
    int32_t tsScaled = (int32_t)((tsNow * (*vddaMvOut)) / VREFINT_CAL_VREF);
    int32_t degC     = ((tsScaled - tsCal1)
                        * (TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP))
                       / (tsCal2 - tsCal1)
                       + TEMPSENSOR_CAL1_TEMP;
    *dieTempCOut = (int16_t)degC;
}

uint32_t adcReadVddaMv(void)
{
    uint32_t vdda;
    int16_t  temp;
    adcReadDiagnostics(&vdda, &temp);
    (void)temp;
    return vdda;
}

int16_t adcReadDieTempC(void)
{
    uint32_t vdda;
    int16_t  temp;
    adcReadDiagnostics(&vdda, &temp);
    (void)vdda;
    return temp;
}

/* -------------------------------------------------------------------------- */
/*                       MSP (low-level HW init)                              */
/* -------------------------------------------------------------------------- */

/**
  * @brief  ADC peripheral GPIO + clock setup.  Split out from the DMA hookup
  *         to keep each function under 20 lines.
  * @retval None
  */
static void adcMspGpioClockInit(void)
{
    GPIO_InitTypeDef         gpio          = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection    = RCC_ADCCLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin  = ADC_SENSOR_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ADC_SENSOR_PORT, &gpio);
}

/**
  * @brief  DMA1 channel 1 setup linked to ADC handle, plus NVIC priority.
  * @param  adcHandle  HAL ADC handle to link the DMA into
  * @retval None
  */
static void adcMspDmaInit(ADC_HandleTypeDef *adcHandle)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdmaAdc.Instance                 = DMA1_Channel1;
    hdmaAdc.Init.Request             = DMA_REQUEST_ADC1;
    hdmaAdc.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdmaAdc.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdmaAdc.Init.MemInc              = DMA_MINC_ENABLE;
    hdmaAdc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdmaAdc.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdmaAdc.Init.Mode                = DMA_NORMAL;
    hdmaAdc.Init.Priority            = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdmaAdc) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_LINKDMA(adcHandle, DMA_Handle, hdmaAdc);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, DMA_ADC_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
    if (adcHandle->Instance == ADC1)
    {
        adcMspGpioClockInit();
        adcMspDmaInit(adcHandle);
    }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adcHandle)
{
    if (adcHandle->Instance == ADC1)
    {
        __HAL_RCC_ADC_CLK_DISABLE();
        HAL_GPIO_DeInit(ADC_SENSOR_PORT, ADC_SENSOR_PIN);

        if (adcHandle->DMA_Handle != NULL)
        {
            HAL_DMA_DeInit(adcHandle->DMA_Handle);
        }
        HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    }
}

/**
  * @brief  TIM6 base MSP -- enable peripheral clock.  Required by HAL when
  *         we call HAL_TIM_Base_Init(&htim6).
  * @param  htim  HAL TIM handle
  * @retval None
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_ENABLE();
    }
}

/**
  * @brief  TIM6 base MSP de-init -- disable peripheral clock.
  * @param  htim  HAL TIM handle
  * @retval None
  */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_DISABLE();
    }
}

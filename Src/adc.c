/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "adc.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;

/* ADC1 init function */
void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance = ADC1;
  /* Change from DIV2 to DIV4 for better internal channel sampling */
  /* DIV4: 16MHz ADC clock, 160.5 cycles = 10µs (suitable for temperature sensor) */
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5;
  hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
	
	
	
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN ADC1_Init 2 */
	HAL_ADCEx_Calibration_Start(&hadc1);
	
	/* Note: VREFINT is enabled by default in STM32G0 series */
	/* No additional enable function needed */
  /* USER CODE END ADC1_Init 2 */

}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC1 clock enable */
    __HAL_RCC_ADC_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();


    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC_CLK_DISABLE();

    /**ADC1 GPIO Configuration
    PA0     ------> ADC1_IN0
    PA1     ------> ADC1_IN1
    PA2     ------> ADC1_IN2
    PA3     ------> ADC1_IN3
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1);

    /* ADC1 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/**
  * @brief  Read internal VREFINT channel
  * @param  None
  * @retval VREFINT ADC raw value (12-bit)
  * @note   Multiple samples are averaged to reduce noise
  */
uint32_t adcReadVrefint(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t vrefintSum = 0;
  const uint8_t SAMPLE_COUNT = 16;
  const uint8_t DUMMY_READS = 5;  /* More dummy reads */
  
  /* Stop ADC before reconfiguring */
  HAL_ADC_Stop(&hadc1);
  
  /* Configure VREFINT channel */
  /* VREFINT requires longer sampling time due to high internal impedance */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;  /* Use longer sampling time */
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
  
  /* Wait longer for channel stabilization */
  HAL_Delay(10);
  
  /* Multiple dummy reads to ensure channel is fully switched */
  for(uint8_t i = 0; i < DUMMY_READS; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    HAL_Delay(5);
  }
  
  /* Average multiple samples */
  for(uint8_t i = 0; i < SAMPLE_COUNT; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 50);
    vrefintSum += HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
  }
  
  return (vrefintSum / SAMPLE_COUNT);
}

/**
  * @brief  Calculate real VDDA voltage using VREFINT
  * @param  None
  * @retval Real VDDA voltage in volts (float)
  * @note   Formula: VDDA = 3.0V * VREFINT_CAL / VREFINT_DATA
  *         This compensates for temperature and supply voltage drift
  */
/**
  * @brief  Debug function to get VREFINT raw values
  * @param  vrefintAdc: pointer to store VREFINT ADC value
  * @param  vrefintCal: pointer to store factory calibration value
  * @param  vddaCalc: pointer to store calculated VDDA
  * @retval None
  */
void adcDebugVrefint(uint32_t *vrefintAdc, uint16_t *vrefintCal, uint16_t *vddaCalc)
{
  uint32_t vrefintData;
  uint16_t calValue;
  float vddaVoltage;
  
  vrefintData = adcReadVrefint();
  calValue = *VREFINT_CAL_ADDR;
  
  /* Calculate VDDA */
  vddaVoltage = ((float)VREFINT_CAL_VREF * (float)calValue) / (float)vrefintData / 1000.0f;
  
  /* Return values */
  *vrefintAdc = vrefintData;
  *vrefintCal = calValue;
  *vddaCalc = (uint16_t)(vddaVoltage * 1000);  /* Convert to mV */
}

float adcGetVdda(void)
{
  uint32_t vrefintData;
  uint16_t vrefintCal;
  float vddaVoltage;
  
  vrefintData = adcReadVrefint();
  vrefintCal = *VREFINT_CAL_ADDR;
  
  /* Calculate real VDDA */
  vddaVoltage = ((float)VREFINT_CAL_VREF * (float)vrefintCal) / (float)vrefintData / 1000.0f;
  
  return vddaVoltage;
}

/**
  * @brief  Read internal temperature sensor
  * @param  None
  * @retval Temperature in 0.1°C units (e.g., 255 = 25.5°C)
  * @note   STM32G070 per datasheet Table 55:
  *         - Single-point calibration: TS_CAL1 at 30°C (V30=0.76V typ)
  *         - Avg_Slope: 2.5 mV/°C (typ, range 2.3-2.7)
  *         - Min sampling time: 5µs
  *         Formula: T(°C) = 30 + (TS_DATA - TS_CAL1) / slope_ADC
  */
int16_t adcReadTemperature(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t tempSum = 0;
  const uint8_t SAMPLE_COUNT = 32;  /* Reduced for faster response */
  uint32_t tempData;
  float temperature;
  
  /* STM32G070 single-point calibration per datasheet */
  uint16_t *TS_CAL1 = (uint16_t*)0x1FFF75A8;
  uint16_t tsCal1 = *TS_CAL1;
  
  /* Datasheet: Avg_Slope = 2.5 mV/°C (typ, 2.3-2.7) */
  #define TEMP_SLOPE_MV_PER_C  2.5f
  
  /* Configure temperature sensor channel */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;  /* 160.5 cycles */
  
  /* Stop ADC before reconfiguring */
  HAL_ADC_Stop(&hadc1);
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
  
  /* Wait for temperature sensor stabilization (datasheet: typ 70µs, max 120µs) */
  HAL_Delay(1);
  
  /* Discard first few samples after channel switch */
  for(uint8_t i = 0; i < 3; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    HAL_ADC_GetValue(&hadc1);
  }
  
  /* Read and average multiple samples */
  for(uint8_t i = 0; i < SAMPLE_COUNT; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    tempSum += HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);
  
  tempData = tempSum / SAMPLE_COUNT;
  
  /* Get current VDDA (critical for accurate temperature calculation) */
  float currentVdda = adcGetVdda();
  
  /* Normalize TS_DATA to 3.0V reference per datasheet calibration condition */
  /* TS_DATA_3V = TS_DATA × (VDDA_actual / 3.0) */
  float tsData3V = (float)tempData * (currentVdda / 3.0f);
  
  /* Calculate slope in ADC counts per °C at 3.0V reference */
  /* slope_ADC = Avg_Slope × 4095 / 3000 = 2.5 × 4095 / 3000 = 3.41 LSB/°C */
  float slopeADC = TEMP_SLOPE_MV_PER_C * 4095.0f / 3000.0f;
  
  /* Calculate temperature per datasheet formula */
  /* T(°C) = 30 + (TS_CAL1 - TS_DATA_3V) / slope_ADC */
  temperature = 30.0f + ((float)tsCal1 - tsData3V) / slopeADC;
  
  /* Return temperature in 0.1°C units */
  return (int16_t)(temperature * 10.0f);
}

/**
  * @brief  Debug function to get temperature sensor raw values
  * @param  tsCal1: pointer to store 30°C calibration value
  * @param  tsCal2: pointer to store slope value (×1000)
  * @param  tsData: pointer to store current ADC reading
  * @param  tempCalc: pointer to store calculated temperature (0.1°C units)
  * @retval None
  */
void adcDebugTemperature(uint16_t *tsCal1, uint16_t *tsCal2, uint32_t *tsData, int16_t *tempCalc)
{
  /* Read TS_CAL1 from Flash */
  uint16_t *TS_CAL1 = (uint16_t*)0x1FFF75A8;
  uint16_t cal1 = *TS_CAL1;
  
  /* Configure and read temperature sensor */
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
  
  HAL_ADC_Stop(&hadc1);
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
  HAL_Delay(1);
  
  /* Discard first samples */
  for(uint8_t i = 0; i < 3; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    HAL_ADC_GetValue(&hadc1);
  }
  
  /* Read 32 samples */
  uint32_t tempSum = 0;
  for(uint8_t i = 0; i < 32; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    tempSum += HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);
  
  uint32_t tempData = tempSum / 32;
  
  /* Get current VDDA and normalize to 3.0V */
  float currentVdda = adcGetVdda();
  float tsData3V = (float)tempData * (currentVdda / 3.0f);
  
  /* Calculate temperature with proper normalization */
  #define TEMP_SLOPE_MV_PER_C  2.5f
  float slopeADC = TEMP_SLOPE_MV_PER_C * 4095.0f / 3000.0f;
  float temperature = 30.0f + ((float)cal1 - tsData3V) / slopeADC;
  
  /* Return values */
  *tsCal1 = cal1;
  *tsCal2 = (uint16_t)(TEMP_SLOPE_MV_PER_C * 1000);
  *tsData = tempData;
  *tempCalc = (int16_t)(temperature * 10.0f);
}
/* USER CODE END 1 */


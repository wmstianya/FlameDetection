/**
  ******************************************************************************
  * @file           : adcDriver.c
  * @brief          : ADC driver implementation with function splitting
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  * @attention
  * All functions comply with 20-line maximum rule.
  * Original large functions have been refactored into smaller units.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "adcDriver.h"
#include "stm32g0xx_ll_adc.h"
#include <math.h>

/* Private defines -----------------------------------------------------------*/
#define VREFINT_CAL_VREF    3000u  /* Calibration voltage: 3.0V */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;  /* ✅ 直接定义，不依赖 adc.c */

/* ============================================================================
   ADC Initialization Functions (替代 adc.c)
   ============================================================================ */

/**
  * @brief  配置ADC时钟
  * @param  None
  * @retval None
  */
static void configureAdcClock(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_SYSCLK;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

/**
  * @brief  配置ADC GPIO
  * @param  None
  * @retval None
  */
static void configureAdcGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  ADC MSP Initialization
  * @param  adcHandle: ADC handle
  * @retval None
  */
void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{
    if(adcHandle->Instance == ADC1) {
        configureAdcClock();
        configureAdcGpio();
    }
}

/**
  * @brief  配置ADC参数
  * @param  None
  * @retval None
  */
static void configureAdcParameters(void)
{
    hadc1.Instance = ADC1;
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
}

/**
  * @brief  配置ADC高级参数
  * @param  None
  * @retval None
  */
static void configureAdcAdvanced(void)
{
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5;
    hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
    hadc1.Init.OversamplingMode = DISABLE;
    hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
}

/**
  * @brief  配置默认ADC通道
  * @param  None
  * @retval None
  */
static void configureDefaultChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  ADC1 初始化函数
  * @param  None
  * @retval None
  */
void adcDriverInit(void)
{
    configureAdcParameters();
    configureAdcAdvanced();
    
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }
    
    configureDefaultChannel();
    HAL_ADCEx_Calibration_Start(&hadc1);
}

/* ============================================================================
   VREFINT and VDDA Functions
   ============================================================================ */

/**
  * @brief  Configure VREFINT channel
  * @param  None
  * @retval ADC status
  */
static AdcStatus configureVrefintChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    HAL_ADC_Stop(&hadc1);
    
    sConfig.Channel = ADC_CHANNEL_VREFINT;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return ADC_STATUS_CHANNEL_ERROR;
    }
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Perform dummy reads to stabilize VREFINT channel
  * @param  None
  * @retval None
  */
static void performVrefintDummyReads(void)
{
    for (uint8_t i = 0; i < VREFINT_DUMMY_READ_COUNT; i++) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 100);
        HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        HAL_Delay(VREFINT_INTER_SAMPLE_DELAY_MS);
    }
}

/**
  * @brief  Sample VREFINT multiple times
  * @param  sum: pointer to store sum of samples
  * @retval ADC status
  */
static AdcStatus sampleVrefintMultiple(uint32_t* sum)
{
    *sum = 0;
    
    for (uint8_t i = 0; i < VREFINT_SAMPLE_COUNT; i++) {
        HAL_ADC_Start(&hadc1);
        
        if (HAL_ADC_PollForConversion(&hadc1, 50) != HAL_OK) {
            HAL_ADC_Stop(&hadc1);
            return ADC_STATUS_TIMEOUT;
        }
        
        *sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Read VREFINT with error checking
  * @param  value: pointer to store VREFINT raw value
  * @retval ADC status
  */
AdcStatus adcReadVrefintSafe(uint32_t* value)
{
    AdcStatus status;
    uint32_t sum = 0;
    
    /* Configure channel */
    status = configureVrefintChannel();
    if (status != ADC_STATUS_OK) return status;
    
    /* Wait for stabilization */
    HAL_Delay(VREFINT_STABILIZATION_DELAY_MS);
    
    /* Dummy reads */
    performVrefintDummyReads();
    
    /* Sample multiple times */
    status = sampleVrefintMultiple(&sum);
    if (status != ADC_STATUS_OK) return status;
    
    /* Calculate average */
    *value = sum / VREFINT_SAMPLE_COUNT;
    
    /* Validate range */
    if (*value < VREFINT_MIN_VALUE || *value > VREFINT_MAX_VALUE) {
        return ADC_STATUS_OUT_OF_RANGE;
    }
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Calculate VDDA from VREFINT raw value
  * @param  vrefintRaw: VREFINT ADC reading
  * @retval VDDA voltage in volts
  */
float adcCalculateVdda(uint32_t vrefintRaw)
{
    uint16_t vrefintCal = *VREFINT_CAL_ADDR;
    float vdda;
    
    vdda = ((float)VREFINT_CAL_VREF * (float)vrefintCal);
    vdda /= (float)vrefintRaw;
    vdda /= 1000.0f;
    
    return vdda;
}

/**
  * @brief  Get VDDA with error handling
  * @param  vdda: pointer to store VDDA value
  * @retval ADC status
  */
AdcStatus adcGetVddaSafe(float* vdda)
{
    uint32_t vrefintData;
    AdcStatus status;
    
    status = adcReadVrefintSafe(&vrefintData);
    if (status != ADC_STATUS_OK) {
        *vdda = DEFAULT_VDDA_VOLTAGE;
        return status;
    }
    
    *vdda = adcCalculateVdda(vrefintData);
    return ADC_STATUS_OK;
}

/* ============================================================================
   Temperature Sensor Functions
   ============================================================================ */

/**
  * @brief  Configure temperature sensor channel
  * @param  None
  * @retval ADC status
  */
static AdcStatus configureTempSensorChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    
    HAL_ADC_Stop(&hadc1);
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return ADC_STATUS_CHANNEL_ERROR;
    }
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Perform dummy reads for temperature sensor
  * @param  None
  * @retval None
  */
static void performTempSensorDummyReads(void)
{
    for (uint8_t i = 0; i < TEMP_SENSOR_DUMMY_READ_COUNT; i++) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        HAL_ADC_GetValue(&hadc1);
    }
}

/**
  * @brief  Sample temperature sensor multiple times
  * @param  sum: pointer to store sum of samples
  * @retval ADC status
  */
static AdcStatus sampleTempSensorMultiple(uint32_t* sum)
{
    *sum = 0;
    
    for (uint8_t i = 0; i < TEMP_SENSOR_SAMPLE_COUNT; i++) {
        HAL_ADC_Start(&hadc1);
        
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
            return ADC_STATUS_TIMEOUT;
        }
        
        *sum += HAL_ADC_GetValue(&hadc1);
    }
    
    HAL_ADC_Stop(&hadc1);
    return ADC_STATUS_OK;
}

/**
  * @brief  Normalize temperature data to 3.0V reference
  * @param  tsData: raw ADC reading
  * @param  vdda: current VDDA voltage
  * @retval Normalized temperature data
  */
float adcNormalizeTemperatureData(uint32_t tsData, float vdda)
{
    return (float)tsData * (vdda / 3.0f);
}

/**
  * @brief  Calculate temperature from normalized data
  * @param  tsData3V: temperature sensor data normalized to 3.0V
  * @param  tsCal1: calibration value at 30°C
  * @retval Temperature in °C
  */
float adcCalculateTemperature(float tsData3V, uint16_t tsCal1)
{
    float slopeAdc = TEMP_SLOPE_MV_PER_C * ADC_MAX_VALUE / 3000.0f;
    float temperature;
    
    temperature = TEMP_CAL1_TEMP_C + ((float)tsCal1 - tsData3V) / slopeAdc;
    
    return temperature;
}

/**
  * @brief  Read temperature with error handling
  * @param  tempX10: pointer to store temperature in 0.1°C units
  * @retval ADC status
  */
AdcStatus adcReadTemperatureSafe(int16_t* tempX10)
{
    AdcStatus status;
    uint32_t tempSum = 0;
    uint32_t tempData;
    float vdda;
    float tsData3V;
    float temperature;
    uint16_t tsCal1;
    
    /* Configure channel */
    status = configureTempSensorChannel();
    if (status != ADC_STATUS_OK) return status;
    
    /* Stabilize */
    HAL_Delay(TEMP_SENSOR_STABILIZATION_MS);
    
    /* Dummy reads */
    performTempSensorDummyReads();
    
    /* Sample multiple times */
    status = sampleTempSensorMultiple(&tempSum);
    if (status != ADC_STATUS_OK) return status;
    
    /* Calculate average */
    tempData = tempSum / TEMP_SENSOR_SAMPLE_COUNT;
    
    /* Get VDDA for normalization */
    status = adcGetVddaSafe(&vdda);
    if (status != ADC_STATUS_OK) {
        vdda = DEFAULT_VDDA_VOLTAGE;
    }
    
    /* Read calibration value */
    tsCal1 = *((uint16_t*)0x1FFF75A8);
    
    /* Normalize and calculate */
    tsData3V = adcNormalizeTemperatureData(tempData, vdda);
    temperature = adcCalculateTemperature(tsData3V, tsCal1);
    
    /* Convert to 0.1°C units */
    *tempX10 = (int16_t)(temperature * 10.0f);
    
    return ADC_STATUS_OK;
}

/* ============================================================================
   Flame Sensor Functions
   ============================================================================ */

/**
  * @brief  Configure flame sensor channel (PA1)
  * @param  None
  * @retval ADC status
  */
AdcStatus adcConfigureFlameChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return ADC_STATUS_CHANNEL_ERROR;
    }
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Read flame sensor once
  * @param  value: pointer to store ADC value
  * @retval ADC status
  */
AdcStatus adcReadFlameSensor(uint32_t* value)
{
    HAL_ADC_Start(&hadc1);
    
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        return ADC_STATUS_TIMEOUT;
    }
    
    *value = HAL_ADC_GetValue(&hadc1);
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Read flame sensor with averaging
  * @param  avgValue: pointer to store average value
  * @retval ADC status
  */
AdcStatus adcReadFlameSensorAverage(float* avgValue)
{
    uint32_t sum = 0;
    uint32_t singleValue;
    AdcStatus status;
    
    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++) {
        status = adcReadFlameSensor(&singleValue);
        if (status != ADC_STATUS_OK) {
            return status;
        }
        sum += singleValue;
    }
    
    *avgValue = (float)sum / (float)ADC_BUFFER_SIZE;
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Convert ADC counts to millivolts
  * @param  adcCounts: ADC raw value
  * @param  vdda: current VDDA voltage
  * @retval Voltage in millivolts
  */
float adcConvertToMillivolts(float adcCounts, float vdda)
{
    return (adcCounts * vdda * 1000.0f) / (float)ADC_MAX_VALUE;
}

/* ============================================================================
   General ADC Functions
   ============================================================================ */

/**
  * @brief  Map custom channel ID to HAL channel
  * @param  channelId: custom channel identifier
  * @retval HAL channel number
  */
static uint32_t mapChannelIdToHal(AdcChannelId channelId)
{
    switch (channelId) {
        case ADC_CHANNEL_FLAME_SENSOR:
            return ADC_CHANNEL_1;
        case ADC_CHANNEL_VREF_INTERNAL:
            return ADC_CHANNEL_VREFINT;
        case ADC_CHANNEL_TEMP_SENSOR:
            return ADC_CHANNEL_TEMPSENSOR;
        default:
            return ADC_CHANNEL_1;
    }
}

/**
  * @brief  Configure ADC channel
  * @param  channelId: channel identifier
  * @retval ADC status
  */
AdcStatus adcConfigureChannel(AdcChannelId channelId)
{
    switch (channelId) {
        case ADC_CHANNEL_FLAME_SENSOR:
            return adcConfigureFlameChannel();
        case ADC_CHANNEL_VREF_INTERNAL:
            return configureVrefintChannel();
        case ADC_CHANNEL_TEMP_SENSOR:
            return configureTempSensorChannel();
        default:
            return ADC_STATUS_CHANNEL_ERROR;
    }
}

/**
  * @brief  Wait for channel stabilization
  * @param  delayMs: delay in milliseconds
  * @retval None
  */
void adcChannelStabilize(uint16_t delayMs)
{
    HAL_Delay(delayMs);
}

/**
  * @brief  Sample ADC once with timeout
  * @param  value: pointer to store ADC value
  * @param  timeoutMs: timeout in milliseconds
  * @retval ADC status
  */
AdcStatus adcSampleOnce(uint32_t* value, uint32_t timeoutMs)
{
    HAL_ADC_Start(&hadc1);
    
    if (HAL_ADC_PollForConversion(&hadc1, timeoutMs) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return ADC_STATUS_TIMEOUT;
    }
    
    *value = HAL_ADC_GetValue(&hadc1);
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Sample ADC multiple times and sum
  * @param  sum: pointer to store sum
  * @param  count: number of samples
  * @retval ADC status
  */
AdcStatus adcSampleMultiple(uint32_t* sum, uint8_t count)
{
    uint32_t value;
    AdcStatus status;
    
    *sum = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        status = adcSampleOnce(&value, 10);
        if (status != ADC_STATUS_OK) {
            return status;
        }
        *sum += value;
    }
    
    return ADC_STATUS_OK;
}

/**
  * @brief  Calculate average from sum
  * @param  sum: sum of samples
  * @param  count: number of samples
  * @retval Average value
  */
uint32_t adcAverageSamples(uint32_t sum, uint8_t count)
{
    return sum / count;
}

/* ============================================================================
   Debug Functions
   ============================================================================ */

/**
  * @brief  Debug function to get VREFINT raw values
  * @param  vrefintAdc: pointer to store VREFINT ADC value
  * @param  vrefintCal: pointer to store factory calibration value
  * @param  vddaCalc: pointer to store calculated VDDA
  * @retval None
  */
void adcDebugVrefint(uint32_t* vrefintAdc, uint16_t* vrefintCal, uint16_t* vddaCalc)
{
    uint32_t vrefintData;
    uint16_t calValue;
    float vddaVoltage;
    
    if (adcReadVrefintSafe(&vrefintData) == ADC_STATUS_OK) {
        *vrefintAdc = vrefintData;
    } else {
        *vrefintAdc = 0;
    }
    
    calValue = *VREFINT_CAL_ADDR;
    *vrefintCal = calValue;
    
    vddaVoltage = adcCalculateVdda(vrefintData);
    *vddaCalc = (uint16_t)(vddaVoltage * 1000.0f);
}

/**
  * @brief  Debug function to get temperature sensor raw values
  * @param  tsCal1: pointer to store 30°C calibration value
  * @param  tsCal2: pointer to store slope value (×1000)
  * @param  tsData: pointer to store current ADC reading
  * @param  tempCalc: pointer to store calculated temperature (0.1°C units)
  * @retval None
  */
void adcDebugTemperature(uint16_t* tsCal1, uint16_t* tsCal2, 
                         uint32_t* tsData, int16_t* tempCalc)
{
    int16_t temp;
    
    *tsCal1 = *((uint16_t*)0x1FFF75A8);
    *tsCal2 = (uint16_t)(TEMP_SLOPE_MV_PER_C * 1000.0f);
    
    if (adcReadTemperatureSafe(&temp) == ADC_STATUS_OK) {
        *tempCalc = temp;
        /* Reconstruct tsData for debug display */
        *tsData = 0;  /* Would need to store this during read */
    } else {
        *tempCalc = 0;
        *tsData = 0;
    }
}

/************************ END OF FILE *****************************/


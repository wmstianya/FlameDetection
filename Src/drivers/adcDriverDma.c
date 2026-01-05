/**
  ******************************************************************************
  * @file           : adcDriverDma.c
  * @brief          : ADC DMA driver implementation (no interrupt, polling)
  * @author         : Senior Embedded Engineer
  * @date           : 2026-01-05
  * @version        : V1.0.5
  ******************************************************************************
  * @attention
  * 实现ADC DMA采集，无中断，通过轮询检测传输完成。
  * 所有函数遵循20行规则。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "adcDriverDma.h"
#include "stm32g0xx_ll_adc.h"
#include <string.h>

/* 外部看门狗句柄 (定义在main.c) */
extern IWDG_HandleTypeDef hiwdg;

/* Private defines -----------------------------------------------------------*/
/* 使用LL库中定义的VREFINT_CAL_ADDR (如果未定义则手动定义) */
#ifndef VREFINT_CAL_ADDR
#define VREFINT_CAL_ADDR        ((uint16_t*)0x1FFF75AA)
#endif

/* VREFINT校准电压 (防止与LL库重复定义) */
#ifndef ADC_DMA_VREFINT_CAL_VREF
#define ADC_DMA_VREFINT_CAL_VREF    3000u   /* 校准电压3.0V */
#endif

/* 温度传感器校准地址 */
#ifndef TS_CAL1_ADDR
#define TS_CAL1_ADDR            ((uint16_t*)0x1FFF75A8)
#endif

/* Private variables ---------------------------------------------------------*/
static ADC_HandleTypeDef hadcDma;
static DMA_HandleTypeDef hdmaDmaAdc;

/* DMA缓冲区 - 使用__ALIGNED确保4字节对齐 */
#if defined(__ARMCC_VERSION)
__align(4) static uint16_t flameDmaBuffer[ADC_DMA_BUFFER_SIZE];
__align(4) static uint16_t vrefintDmaBuffer[VREFINT_DMA_SAMPLE_COUNT];
__align(4) static uint16_t tempDmaBuffer[TEMP_DMA_SAMPLE_COUNT];
#elif defined(__GNUC__)
static uint16_t flameDmaBuffer[ADC_DMA_BUFFER_SIZE] __attribute__((aligned(4)));
static uint16_t vrefintDmaBuffer[VREFINT_DMA_SAMPLE_COUNT] __attribute__((aligned(4)));
static uint16_t tempDmaBuffer[TEMP_DMA_SAMPLE_COUNT] __attribute__((aligned(4)));
#else
static uint16_t flameDmaBuffer[ADC_DMA_BUFFER_SIZE];
static uint16_t vrefintDmaBuffer[VREFINT_DMA_SAMPLE_COUNT];
static uint16_t tempDmaBuffer[TEMP_DMA_SAMPLE_COUNT];
#endif

/* 当前VDDA电压 */
static float currentVdda = DEFAULT_VDDA_VOLTAGE;

/* 采样状态标志 */
static volatile uint8_t dmaTransferBusy = 0;

/* ============================================================================
   DMA配置函数
   ============================================================================ */

/**
  * @brief  配置DMA通道
  */
static void configureDmaChannel(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    hdmaDmaAdc.Instance = DMA1_Channel1;
    hdmaDmaAdc.Init.Request = DMA_REQUEST_ADC1;
    hdmaDmaAdc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdmaDmaAdc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdmaDmaAdc.Init.MemInc = DMA_MINC_ENABLE;
    hdmaDmaAdc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdmaDmaAdc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdmaDmaAdc.Init.Mode = DMA_NORMAL;
    hdmaDmaAdc.Init.Priority = DMA_PRIORITY_HIGH;
    
    HAL_DMA_Init(&hdmaDmaAdc);
    __HAL_LINKDMA(&hadcDma, DMA_Handle, hdmaDmaAdc);
}

/**
  * @brief  配置ADC GPIO (PA1)
  */
static void configureAdcGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  配置ADC时钟
  */
static void configureAdcClock(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_SYSCLK;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    
    __HAL_RCC_ADC_CLK_ENABLE();
}

/**
  * @brief  配置ADC基础参数
  */
static void configureAdcBase(void)
{
    hadcDma.Instance = ADC1;
    hadcDma.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadcDma.Init.Resolution = ADC_RESOLUTION_12B;
    hadcDma.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadcDma.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadcDma.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadcDma.Init.LowPowerAutoWait = DISABLE;
    hadcDma.Init.LowPowerAutoPowerOff = DISABLE;
    hadcDma.Init.ContinuousConvMode = ENABLE;
    hadcDma.Init.NbrOfConversion = 1;
    hadcDma.Init.DiscontinuousConvMode = DISABLE;
    hadcDma.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadcDma.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadcDma.Init.DMAContinuousRequests = ENABLE;
    hadcDma.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadcDma.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5;
    hadcDma.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
    hadcDma.Init.OversamplingMode = DISABLE;
    hadcDma.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
}

/**
  * @brief  ADC初始化
  */
void adcDmaInit(void)
{
    configureAdcClock();
    configureAdcGpio();
    configureAdcBase();
    
    if (HAL_ADC_Init(&hadcDma) != HAL_OK) {
        Error_Handler();
    }
    
    configureDmaChannel();
    HAL_ADCEx_Calibration_Start(&hadcDma);
    
    /* 使能VREFINT */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    
    /* 初始化VDDA */
    adcDmaUpdateVdda();
}

/**
  * @brief  ADC反初始化
  */
void adcDmaDeInit(void)
{
    HAL_ADC_DeInit(&hadcDma);
    HAL_DMA_DeInit(&hdmaDmaAdc);
}

/* ============================================================================
   通道配置函数
   ============================================================================ */

/**
  * @brief  配置火焰传感器通道(PA1)
  */
static AdcDmaStatus configureFlameChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    HAL_ADC_Stop_DMA(&hadcDma);
    
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    
    if (HAL_ADC_ConfigChannel(&hadcDma, &sConfig) != HAL_OK) {
        return ADC_DMA_ERROR;
    }
    return ADC_DMA_OK;
}

/**
  * @brief  配置VREFINT通道
  */
static AdcDmaStatus configureVrefintChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    HAL_ADC_Stop_DMA(&hadcDma);
    
    sConfig.Channel = ADC_CHANNEL_VREFINT;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    
    if (HAL_ADC_ConfigChannel(&hadcDma, &sConfig) != HAL_OK) {
        return ADC_DMA_ERROR;
    }
    return ADC_DMA_OK;
}

/**
  * @brief  配置温度传感器通道
  */
static AdcDmaStatus configureTempChannel(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    HAL_ADC_Stop_DMA(&hadcDma);
    
    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    
    if (HAL_ADC_ConfigChannel(&hadcDma, &sConfig) != HAL_OK) {
        return ADC_DMA_ERROR;
    }
    return ADC_DMA_OK;
}

/**
  * @brief  切换ADC通道
  */
AdcDmaStatus adcDmaSwitchChannel(AdcChannelType channel)
{
    switch (channel) {
        case ADC_CH_FLAME_SENSOR:
            return configureFlameChannel();
        case ADC_CH_VREFINT:
            return configureVrefintChannel();
        case ADC_CH_TEMP_SENSOR:
            return configureTempChannel();
        default:
            return ADC_DMA_ERROR;
    }
}

/* ============================================================================
   DMA采样函数 (轮询模式，无中断)
   ============================================================================ */

/**
  * @brief  启动DMA采样并轮询等待完成
  * @note   在轮询过程中刷新看门狗，防止超时复位
  */
static AdcDmaStatus startDmaAndPoll(uint16_t* buffer, uint16_t size)
{
    uint32_t startTick = HAL_GetTick();
    uint32_t lastWdgRefresh = startTick;
    
    /* 空指针检查 */
    if (buffer == NULL || size == 0) {
        return ADC_DMA_ERROR;
    }
    
    /* 清空缓冲区 */
    memset(buffer, 0, size * sizeof(uint16_t));
    
    /* 启动DMA传输 */
    if (HAL_ADC_Start_DMA(&hadcDma, (uint32_t*)buffer, size) != HAL_OK) {
        return ADC_DMA_ERROR;
    }
    
    /* 轮询等待DMA完成 */
    while (__HAL_DMA_GET_COUNTER(&hdmaDmaAdc) > 0) {
        uint32_t nowTick = HAL_GetTick();
        
        /* 超时检查 */
        if ((nowTick - startTick) > ADC_DMA_TIMEOUT_MS) {
            HAL_ADC_Stop_DMA(&hadcDma);
            return ADC_DMA_TIMEOUT;
        }
        
        /* 每50ms刷新一次看门狗 */
        if ((nowTick - lastWdgRefresh) >= 50) {
            HAL_IWDG_Refresh(&hiwdg);
            lastWdgRefresh = nowTick;
        }
    }
    
    HAL_ADC_Stop_DMA(&hadcDma);
    return ADC_DMA_OK;
}

/**
  * @brief  计算缓冲区平均值
  * @param  buffer: 数据缓冲区指针
  * @param  size: 缓冲区大小
  * @retval 平均值，如果buffer为NULL或size为0则返回0
  */
uint32_t adcDmaGetBufferAverage(const uint16_t* buffer, uint16_t size)
{
    uint32_t sum = 0;
    
    /* 空指针检查 */
    if (buffer == NULL || size == 0) {
        return 0;
    }
    
    for (uint16_t i = 0; i < size; i++) {
        sum += buffer[i];
    }
    
    return sum / size;
}

/* ============================================================================
   VREFINT和VDDA函数
   ============================================================================ */

/**
  * @brief  读取VREFINT平均值
  * @param  avgValue: 输出参数，存储平均值
  * @retval ADC_DMA_ERROR如果参数为NULL
  */
AdcDmaStatus adcDmaReadVrefint(uint32_t* avgValue)
{
    AdcDmaStatus status;
    
    /* 空指针检查 */
    if (avgValue == NULL) {
        return ADC_DMA_ERROR;
    }
    
    /* 切换到VREFINT通道 */
    status = configureVrefintChannel();
    if (status != ADC_DMA_OK) return status;
    
    /* 等待通道稳定 */
    HAL_Delay(VREFINT_STABILIZATION_DELAY_MS);
    
    /* DMA采样 */
    status = startDmaAndPoll(vrefintDmaBuffer, VREFINT_DMA_SAMPLE_COUNT);
    if (status != ADC_DMA_OK) return status;
    
    /* 计算平均值 */
    *avgValue = adcDmaGetBufferAverage(vrefintDmaBuffer, VREFINT_DMA_SAMPLE_COUNT);
    
    /* 范围检查 */
    if (*avgValue < VREFINT_MIN_VALUE || *avgValue > VREFINT_MAX_VALUE) {
        return ADC_DMA_OUT_OF_RANGE;
    }
    
    return ADC_DMA_OK;
}

/**
  * @brief  VREFINT温度补偿
  */
float adcDmaCompensateVrefint(uint32_t vrefintRaw, float tempC)
{
    float tempDelta = tempC - VREFINT_CAL_TEMP_C;
    float driftFactor = 1.0f + (tempDelta * VREFINT_TEMP_COEFF_PPM / 1e6f);
    return (float)vrefintRaw * driftFactor;
}

/**
  * @brief  计算VDDA电压
  */
float adcDmaCalculateVdda(uint32_t vrefintAvg)
{
    uint16_t vrefintCal = *VREFINT_CAL_ADDR;
    float vdda;
    
    if (vrefintAvg == 0) {
        return DEFAULT_VDDA_VOLTAGE;
    }
    
    vdda = ((float)ADC_DMA_VREFINT_CAL_VREF * (float)vrefintCal);
    vdda /= (float)vrefintAvg;
    vdda /= 1000.0f;
    
    return vdda;
}

/**
  * @brief  更新当前VDDA
  */
AdcDmaStatus adcDmaUpdateVdda(void)
{
    uint32_t vrefintAvg;
    AdcDmaStatus status;
    
    status = adcDmaReadVrefint(&vrefintAvg);
    if (status == ADC_DMA_OK) {
        currentVdda = adcDmaCalculateVdda(vrefintAvg);
    }
    
    return status;
}

/**
  * @brief  获取当前VDDA
  */
float adcDmaGetCurrentVdda(void)
{
    return currentVdda;
}

/* ============================================================================
   火焰传感器函数
   ============================================================================ */

/**
  * @brief  ADC计数转换为毫伏
  */
float adcDmaCountsToMillivolts(uint32_t counts, float vdda)
{
    return ((float)counts * vdda * 1000.0f) / (float)ADC_MAX_VALUE;
}

/**
  * @brief  读取火焰传感器 (完整流程)
  * @param  result: 输出参数，存储采样结果
  * @retval ADC_DMA_ERROR如果参数为NULL
  */
AdcDmaStatus adcDmaReadFlameSensor(AdcSampleResult* result)
{
    AdcDmaStatus status;
    uint32_t avgRaw;
    
    /* 空指针检查 */
    if (result == NULL) {
        return ADC_DMA_ERROR;
    }
    
    /* 先更新VDDA */
    adcDmaUpdateVdda();
    
    /* 切换到火焰传感器通道 */
    status = configureFlameChannel();
    if (status != ADC_DMA_OK) {
        result->status = status;
        return status;
    }
    
    /* 等待通道稳定 */
    HAL_Delay(2);
    
    /* DMA采样 */
    status = startDmaAndPoll(flameDmaBuffer, ADC_DMA_BUFFER_SIZE);
    if (status != ADC_DMA_OK) {
        result->status = status;
        return status;
    }
    
    /* 计算结果 */
    avgRaw = adcDmaGetBufferAverage(flameDmaBuffer, ADC_DMA_BUFFER_SIZE);
    
    result->rawAverage = avgRaw;
    result->vddaVoltage = currentVdda;
    result->sampleCount = ADC_DMA_BUFFER_SIZE;
    result->voltageMv = adcDmaCountsToMillivolts(avgRaw, currentVdda);
    result->voltageMv *= PA1_HARDWARE_SCALE_FACTOR;
    result->status = ADC_DMA_OK;
    
    return ADC_DMA_OK;
}

/**
  * @brief  启动火焰传感器采样 (非阻塞)
  */
AdcDmaStatus adcDmaStartFlameSampling(void)
{
    AdcDmaStatus status;
    
    if (dmaTransferBusy) {
        return ADC_DMA_BUSY;
    }
    
    status = configureFlameChannel();
    if (status != ADC_DMA_OK) return status;
    
    memset(flameDmaBuffer, 0, sizeof(flameDmaBuffer));
    
    if (HAL_ADC_Start_DMA(&hadcDma, (uint32_t*)flameDmaBuffer, 
                          ADC_DMA_BUFFER_SIZE) != HAL_OK) {
        return ADC_DMA_ERROR;
    }
    
    dmaTransferBusy = 1;
    return ADC_DMA_OK;
}

/**
  * @brief  轮询火焰传感器采样完成
  * @param  result: 输出参数，存储采样结果
  * @retval ADC_DMA_ERROR如果参数为NULL
  */
AdcDmaStatus adcDmaPollFlameSampling(AdcSampleResult* result)
{
    uint32_t avgRaw;
    
    /* 空指针检查 */
    if (result == NULL) {
        return ADC_DMA_ERROR;
    }
    
    if (!dmaTransferBusy) {
        return ADC_DMA_ERROR;
    }
    
    /* 检查DMA是否完成 */
    if (__HAL_DMA_GET_COUNTER(&hdmaDmaAdc) > 0) {
        return ADC_DMA_BUSY;
    }
    
    /* DMA完成，停止并处理数据 */
    HAL_ADC_Stop_DMA(&hadcDma);
    dmaTransferBusy = 0;
    
    avgRaw = adcDmaGetBufferAverage(flameDmaBuffer, ADC_DMA_BUFFER_SIZE);
    
    result->rawAverage = avgRaw;
    result->vddaVoltage = currentVdda;
    result->sampleCount = ADC_DMA_BUFFER_SIZE;
    result->voltageMv = adcDmaCountsToMillivolts(avgRaw, currentVdda);
    result->voltageMv *= PA1_HARDWARE_SCALE_FACTOR;
    result->status = ADC_DMA_OK;
    
    return ADC_DMA_OK;
}

/* ============================================================================
   温度传感器函数
   ============================================================================ */

/**
  * @brief  读取温度传感器
  * @param  tempX10: 输出参数，温度值(0.1°C单位)
  * @retval ADC_DMA_ERROR如果参数为NULL
  */
AdcDmaStatus adcDmaReadTemperature(int16_t* tempX10)
{
    AdcDmaStatus status;
    uint32_t avgRaw;
    float tsData3V;
    float temperature;
    uint16_t tsCal1;
    float slopeAdc;
    
    /* 空指针检查 */
    if (tempX10 == NULL) {
        return ADC_DMA_ERROR;
    }
    
    /* 切换到温度传感器通道 */
    status = configureTempChannel();
    if (status != ADC_DMA_OK) return status;
    
    HAL_Delay(TEMP_SENSOR_STABILIZATION_MS);
    
    /* DMA采样 */
    status = startDmaAndPoll(tempDmaBuffer, TEMP_DMA_SAMPLE_COUNT);
    if (status != ADC_DMA_OK) return status;
    
    avgRaw = adcDmaGetBufferAverage(tempDmaBuffer, TEMP_DMA_SAMPLE_COUNT);
    
    /* 归一化到3.0V */
    tsData3V = (float)avgRaw * (currentVdda / 3.0f);
    
    /* 读取校准值 */
    tsCal1 = *TS_CAL1_ADDR;
    
    /* 计算温度 */
    slopeAdc = TEMP_SLOPE_MV_PER_C * ADC_MAX_VALUE / 3000.0f;
    temperature = TEMP_CAL1_TEMP_C + ((float)tsCal1 - tsData3V) / slopeAdc;
    
    *tempX10 = (int16_t)(temperature * 10.0f);
    
    return ADC_DMA_OK;
}

/* ============================================================================
   调试函数
   ============================================================================ */

/**
  * @brief  调试输出
  */
void adcDmaDebugPrint(void)
{
    /* 预留调试接口 */
}

/************************ END OF FILE *****************************/


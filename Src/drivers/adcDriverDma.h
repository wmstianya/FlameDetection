/**
  ******************************************************************************
  * @file           : adcDriverDma.h
  * @brief          : ADC DMA driver interface (no interrupt, polling mode)
  * @author         : Senior Embedded Engineer
  * @date           : 2026-01-05
  * @version        : V1.0.5
  ******************************************************************************
  * @attention
  * ADC DMA采集驱动 - 无中断轮询模式
  * 特点：
  * 1. DMA自动搬运，CPU占用低
  * 2. 采样时序一致，噪声小
  * 3. 无中断，通过轮询检测完成
  ******************************************************************************
  */

#ifndef __ADC_DRIVER_DMA_H
#define __ADC_DRIVER_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "../config/systemConfig.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  ADC DMA操作状态码
  */
typedef enum {
    ADC_DMA_OK = 0,
    ADC_DMA_BUSY,
    ADC_DMA_TIMEOUT,
    ADC_DMA_ERROR,
    ADC_DMA_OUT_OF_RANGE
} AdcDmaStatus;

/**
  * @brief  ADC通道标识
  */
typedef enum {
    ADC_CH_FLAME_SENSOR = 0,    /* PA1火焰传感器 */
    ADC_CH_VREFINT,             /* 内部参考电压 */
    ADC_CH_TEMP_SENSOR          /* 内部温度传感器 */
} AdcChannelType;

/**
  * @brief  ADC采样结果结构
  */
typedef struct {
    float voltageMv;            /* 计算后的电压值(mV) */
    float vddaVoltage;          /* 当前VDDA电压(V) */
    uint32_t rawAverage;        /* 原始ADC平均值 */
    uint16_t sampleCount;       /* 采样次数 */
    AdcDmaStatus status;        /* 采样状态 */
} AdcSampleResult;

/* Exported functions --------------------------------------------------------*/

/* 初始化 */
void adcDmaInit(void);
void adcDmaDeInit(void);

/* VREFINT和VDDA */
AdcDmaStatus adcDmaReadVrefint(uint32_t* avgValue);
float adcDmaCalculateVdda(uint32_t vrefintAvg);
AdcDmaStatus adcDmaUpdateVdda(void);
float adcDmaGetCurrentVdda(void);

/* VREFINT温度补偿 */
float adcDmaCompensateVrefint(uint32_t vrefintRaw, float tempC);

/* 火焰传感器 */
AdcDmaStatus adcDmaReadFlameSensor(AdcSampleResult* result);
AdcDmaStatus adcDmaStartFlameSampling(void);
AdcDmaStatus adcDmaPollFlameSampling(AdcSampleResult* result);

/* 温度传感器 */
AdcDmaStatus adcDmaReadTemperature(int16_t* tempX10);

/* 通道切换 */
AdcDmaStatus adcDmaSwitchChannel(AdcChannelType channel);

/* 工具函数 */
float adcDmaCountsToMillivolts(uint32_t counts, float vdda);
uint32_t adcDmaGetBufferAverage(const uint16_t* buffer, uint16_t size);

/* 调试接口 */
void adcDmaDebugPrint(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_DRIVER_DMA_H */

/************************ END OF FILE *****************************/


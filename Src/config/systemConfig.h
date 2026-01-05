/**
  ******************************************************************************
  * @file           : systemConfig.h
  * @brief          : System configuration parameters for Flame Detection
  * @author         : Senior Embedded Engineer
  * @date           : 2026-01-05
  * @version        : V1.0.5
  ******************************************************************************
  * @attention
  * Centralized configuration management for all system parameters.
  * V1.0.5: ADC DMA模式优化, 迟滞阈值, VREFINT温度补偿增强
  ******************************************************************************
  */

#ifndef __SYSTEM_CONFIG_H
#define __SYSTEM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"

/* ============================================================================
   ADC Configuration
   ============================================================================ */

/* ADC基础配置 */
#define ADC_RESOLUTION_BITS             12u
#define ADC_MAX_VALUE                   4095u

/* ============================================================================
   ADC DMA配置 (V1.0.5新增)
   ============================================================================ */

/* 使能DMA模式 (1=DMA, 0=轮询) */
#define ADC_USE_DMA_MODE                1u

/* 火焰传感器DMA缓冲区大小 */
#define ADC_DMA_BUFFER_SIZE             128u

/* VREFINT DMA采样次数 */
#define VREFINT_DMA_SAMPLE_COUNT        16u

/* 温度传感器DMA采样次数 */
#define TEMP_DMA_SAMPLE_COUNT           32u

/* DMA传输超时 (ms) */
#define ADC_DMA_TIMEOUT_MS              100u

/* DMA轮询间隔 (ms) */
#define ADC_DMA_POLL_INTERVAL_MS        1u

/* 兼容旧定义 */
#define ADC_BUFFER_SIZE                 ADC_DMA_BUFFER_SIZE

/* VREFINT采样配置 */
#define VREFINT_SAMPLE_COUNT            VREFINT_DMA_SAMPLE_COUNT
#define VREFINT_STABILIZATION_DELAY_MS  5u
#define VREFINT_CHANNEL_SETTLE_US       10u

/* 温度传感器采样配置 */
#define TEMP_SENSOR_SAMPLE_COUNT        TEMP_DMA_SAMPLE_COUNT
#define TEMP_SENSOR_STABILIZATION_MS    1u

/* VREFINT有效范围 */
#define VREFINT_MIN_VALUE               1200u
#define VREFINT_MAX_VALUE               1800u

/* VREFINT温度补偿系数 (ppm/°C) */
#define VREFINT_TEMP_COEFF_PPM          30
#define VREFINT_CAL_TEMP_C              30.0f

/* ============================================================================
   Flame Detection Configuration
   ============================================================================ */

/* 火焰检测迟滞阈值 (mV) - V1.0.5优化 */
#define FLAME_THRESH_LOW_MV             2550u   /* 低于此值 = 确认有火焰 */
#define FLAME_THRESH_HIGH_MV            2750u   /* 高于此值 = 确认无火焰 */
#define FLAME_THRESHOLD_MV              2680u   /* 兼容旧代码的中间阈值 */

/* 迟滞区间说明:
 * - 电压 < 2550mV: 确认火焰存在
 * - 2550mV <= 电压 <= 2750mV: 保持上一状态(迟滞区)
 * - 电压 > 2750mV: 确认火焰熄灭
 * 这样可以避免2600mV小火在阈值边缘跳变
 */

/* 时序参数 (ms) */
#define STARTUP_DELAY_MS                2000u   /* 开机后2秒忽略火焰检测 */
#define FLAME_OFF_DELAY_MS              1500u   /* 火焰熄灭确认延迟1.5秒 */
#define FLAME_ON_CONFIRM_MS             100u    /* 火焰点燃确认延迟100ms */

/* Display update intervals (ms) */
#define DISPLAY_REFRESH_PERIOD_MS       50u
#define TEMP_DISPLAY_PERIOD_MS          10000u  /* Show temp every 10s */
#define TEMP_DISPLAY_DURATION_MS        3000u   /* Display temp for 3s */
#define TEMP_DISPLAY_BLANK_MS           500u    /* Blank 0.5s after temp */

/* ============================================================================
   Temperature Compensation Configuration
   ============================================================================ */

/* Temperature compensation settings */
#define TEMP_COMP_REF_C                 30.0f   /* Reference temperature */
#define PA1_TEMP_COEFF_MV_PER_C         (-2.9f) /* Temp coefficient mV/°C */

/* Hardware calibration coefficient */
#define PA1_HARDWARE_SCALE_FACTOR       2.248f  /* 实测2455mV / ADC计算1092mV = 2.248 */
                                                 /* 说明：硬件电路有分压或信号调理 */

/* Auto K adaptation parameters */
#define AUTO_K_ALPHA                    0.0005f     /* Learning rate */
#define AUTO_K_DVALUE_THRESH_MV         3.0f        /* Stability threshold */
#define AUTO_K_STABLE_WINDOW_MS         8000u       /* Stable window */
#define AUTO_K_MIN_K                    (-5.0f)     /* K lower limit */
#define AUTO_K_MAX_K                    (0.0f)      /* K upper limit */
#define AUTO_K_EPS                      0.5f        /* Division protection */
#define AUTO_K_FIXED_REF_MV             2850.0f     /* Fixed reference voltage */

/* ============================================================================
   Task Scheduler Configuration
   ============================================================================ */

/* Task execution periods (ms) */
#define TASK_ADC_SAMPLE_PERIOD_MS       10u
#define TASK_TEMP_SENSE_PERIOD_MS       200u
#define TASK_COMPENSATE_PERIOD_MS       200u
#define TASK_CONTROL_PERIOD_MS          20u
#define TASK_DISPLAY_PERIOD_MS          50u
#define TASK_WATCHDOG_PERIOD_MS         100u

/* ============================================================================
   Debug Configuration
   ============================================================================ */

/* Debug mode switches (set to 1 to enable, 0 to disable) */
#define VREFINT_DEBUG_MODE              0u   /* Display VREFINT debug info */
#define TEMP_DISPLAY_ENABLE             1u   /* Enable temp display every 10s */
#define TEMP_DEBUG_MODE                 0u   /* ✅ 改为0进入正常模式 */
#define TEMP_COMP_ENABLE                0u   /* Temperature compensation */
#define AUTO_K_ENABLE                   1u   /* Auto K adaptation */
#define AUTO_K_USE_FIXED_REF            0u   /* Use fixed reference voltage */

/* Debug display timing */
#define DEBUG_DISPLAY_DURATION_MS       3000u
#define DEBUG_DISPLAY_BLANK_MS          1000u
#define DEBUG_WATCHDOG_REFRESH_MS       100u
#define DEBUG_WATCHDOG_REFRESH_COUNT    30u

/* ============================================================================
   GPIO Pin Definitions
   ============================================================================ */

/* LED and relay control pins */
#define FLAME_LED_PORT                  GPIOA
#define FLAME_LED_PIN                   GPIO_PIN_10

#define FLAME_RELAY_PORT                GPIOA
#define FLAME_RELAY_PIN                 GPIO_PIN_11

/* TM1650 display pins */
#define TM1650_CLK_PORT                 GPIOD
#define TM1650_CLK_PIN                  GPIO_PIN_1

#define TM1650_SDA_PORT                 GPIOD
#define TM1650_SDA_PIN                  GPIO_PIN_2

/* ============================================================================
   System Constants
   ============================================================================ */

/* Default VDDA voltage (volts) */
#define DEFAULT_VDDA_VOLTAGE            3.25f

/* Temperature sensor slope (mV/°C per datasheet) */
#define TEMP_SLOPE_MV_PER_C             2.5f

/* Temperature calibration points */
#define TEMP_CAL1_TEMP_C                30.0f   /* TS_CAL1 at 30°C */

/* IIR filter coefficient for temperature smoothing */
#define TEMP_FILTER_ALPHA               0.1f    /* New sample weight */
#define TEMP_FILTER_BETA                0.9f    /* Old sample weight */

/* Moving average window for temperature display */
#define TEMP_HISTORY_SIZE               3u

/* ============================================================================
   Feature Configuration Structure
   ============================================================================ */

typedef struct {
    /* Flame detection parameters */
    uint16_t flameThresholdMv;
    uint16_t startupDelayMs;
    uint16_t flameOffDelayMs;
    
    /* ADC parameters */
    uint8_t adcBufferSize;
    uint8_t vrefintSampleCount;
    
    /* Temperature compensation */
    float tempRefC;
    float tempCoeffMvPerC;
    uint8_t tempCompEnable;
    uint8_t autoKEnable;
    
    /* Display parameters */
    uint16_t tempDisplayPeriodMs;
    uint16_t tempDisplayDurationMs;
    uint8_t tempDisplayEnable;
    
    /* Debug mode */
    uint8_t debugMode;
} SystemConfig;

/* ============================================================================
   Function Prototypes
   ============================================================================ */

void systemConfigInit(SystemConfig* config);
void systemConfigLoadDefaults(SystemConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_CONFIG_H */

/************************ END OF FILE *****************************/


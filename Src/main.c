/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/*2025年2月119日10:36:20  V1.0.1  开机前2秒，不检测火焰信号， 火焰丢失0.5s后，再给输出信号*/
/*2025年3月31日17:05:11 V1.0.2,修改火焰判定的值，由2500改为2680，熄火判定由0.5秒，改为1.5秒*/
/*2026年1月5日 V1.0.5 ADC DMA模式优化，迟滞阈值，VREFINT温度补偿增强*/

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tm1650.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <math.h>
#include "config/systemConfig.h"
#if ADC_USE_DMA_MODE
#include "drivers/adcDriverDma.h"  /* V1.0.5 DMA模式 */
#else
#include "drivers/adcDriver.h"     /* 旧轮询模式 */
#endif
#include "utils/filter.h"
#include "application/flameDetector.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ? 所有配置已移至 config/systemConfig.h */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;
float v_tmp_f=0.0;
__IO uint32_t   adc_dma_buffer=0;

uint16_t count=0;
uint32_t Value=0;

static  uint16_t FlameOffCount=0;

static  uint16_t FirstStartCount = 0;

/* Runtime k and adaptation state */
#if TEMP_COMP_ENABLE && AUTO_K_ENABLE
static float kRuntime = PA1_TEMP_COEFF_MV_PER_C;
static uint8_t autoKHasTarget = 0;
static float autoKTargetMv = 0.0f;
static uint32_t autoKStableStartMs = 0;
static float lastValueRawMv = -1.0f;
#endif
/* USER CODE BEGIN PV */
/* Cooperative scheduler state */
static uint32_t nextAdcSampleMs = 0;
static uint32_t nextTempReadMs = 0;
static uint32_t nextCompensateMs = 0;
static uint32_t nextControlMs = 0;
static uint32_t nextDisplayMs = 0;
static uint32_t nextWatchdogMs = 0;

/* Shared measurements */
static float measValueRawMv = 0.0f;
static float measTempC = 30.0f;
static float measTempCFiltered = 30.0f;
static float measValueCorrMv = 0.0f;

/* Non-blocking temp display state */
static uint8_t displayInTempPage = 0;
static uint32_t displaySwitchBaseMs = 0;

/* V1.0.5: 火焰迟滞检测状态 */
typedef enum {
    FLAME_HYSTERESIS_UNKNOWN = 0,
    FLAME_HYSTERESIS_PRESENT,   /* 火焰存在 */
    FLAME_HYSTERESIS_ABSENT     /* 火焰熄灭 */
} FlameHysteresisState;

static FlameHysteresisState flameHysteresisState = FLAME_HYSTERESIS_UNKNOWN;
static uint32_t flameOnConfirmStartMs = 0;

#if ADC_USE_DMA_MODE
/* DMA采样结果 */
static AdcSampleResult dmaResult;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
static void taskAdcSample(uint32_t nowMs);
static void taskTempSense(uint32_t nowMs);
static void taskCompensateAndAdapt(uint32_t nowMs);
static void taskControl(uint32_t nowMs);
static void taskDisplay(uint32_t nowMs);
static void taskWatchdog(uint32_t nowMs);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */


/**
  * @brief  Calculate moving average of temperature
  * @param  tempHistory: pointer to temperature history array
  * @retval Average temperature
  */
static int16_t calculateTempAverage(const int16_t* tempHistory)
{
    int32_t tempSum = 0;
    for(int i = 0; i < TEMP_HISTORY_SIZE; i++) {
        tempSum += tempHistory[i];
    }
    return (int16_t)(tempSum / TEMP_HISTORY_SIZE);
}

/**
  * @brief  Display temperature with watchdog refresh
  * @param  displayValue: value to display
  * @retval None
  */
static void displayTempWithWatchdog(uint16_t displayValue)
{
    dis_value(displayValue);
    
    for(int i=0; i<DEBUG_WATCHDOG_REFRESH_COUNT; i++) {
        HAL_Delay(DEBUG_WATCHDOG_REFRESH_MS);
        HAL_IWDG_Refresh(&hiwdg);
    }
    
    dis_value(0);
    HAL_Delay(TEMP_DISPLAY_BLANK_MS);
}

/**
  * @brief  Display internal temperature every 10 seconds
  * @param  None
  * @retval None
  */
void displayTemperaturePeriodic(void)
{
    static uint32_t lastTempTime = 0;
    static int16_t tempHistory[TEMP_HISTORY_SIZE] = {0};
    static uint8_t historyIndex = 0;
    uint32_t currentTime = HAL_GetTick();
    
    if(currentTime - lastTempTime < TEMP_DISPLAY_PERIOD_MS) return;
    
    lastTempTime = currentTime;
    
    int16_t temperatureRaw;
    if (adcReadTemperatureSafe(&temperatureRaw) == ADC_STATUS_OK) {
        tempHistory[historyIndex] = temperatureRaw;
        historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
        
        int16_t temperatureAvg = calculateTempAverage(tempHistory);
        displayTempWithWatchdog((uint16_t)temperatureAvg);
    }
}

/**
  * @brief  The application entry point.
  * @retval int
  */


void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
	
	FlameOffCount++;
	
	FirstStartCount++;
	
	if(FirstStartCount > STARTUP_DELAY_MS) //First start delay, check flameSignal
			FirstStartCount = STARTUP_DELAY_MS;
//	if(FlameOffCount > 200)
//		FlameOffCount = 0;
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
 
#if ADC_USE_DMA_MODE
	adcDmaInit();     /* V1.0.5: DMA模式初始化 */
#else
	adcDriverInit();  /* 旧轮询模式初始化 */
#endif
	
	tm1650_gpio_init();
	MX_IWDG_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		HAL_IWDG_Refresh(&hiwdg);
		
#if VREFINT_DEBUG_MODE
		/* ===== VREFINT DEBUG MODE ===== */
		/* Display VREFINT debug information on 4-digit LED */
		static uint8_t debugStep = 0;
		uint32_t vrefintAdc = 0;
		uint16_t vrefintCal = 0;
		uint16_t vddaCalc = 0;
		
		/* Read VREFINT values */
		adcDebugVrefint(&vrefintAdc, &vrefintCal, &vddaCalc);
		
		/* Display current value based on step */
		if(debugStep == 0)
		{
			/* Display VREFINT ADC raw value (should be 1400-1600) */
			dis_value(vrefintAdc);
		}
		else if(debugStep == 1)
		{
			/* Display factory calibration value */
			dis_value(vrefintCal);
		}
		else if(debugStep == 2)
		{
			/* Display calculated VDDA in mV */
			dis_value(vddaCalc);
		}
		
		/* Wait 3 seconds (with watchdog refresh) */
		for(int i=0; i<30; i++)
		{
			HAL_Delay(100);
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* Clear display (all OFF) for 1 second to indicate switching */
		dis_value(0);
		for(int i=0; i<10; i++)
		{
			HAL_Delay(100);
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* Move to next value */
		debugStep++;
		if(debugStep >= 3)
		{
			debugStep = 0;
		}
		/* ===== END DEBUG MODE ===== */
#elif TEMP_DEBUG_MODE
		/* ===== TEMPERATURE DEBUG MODE ===== */
		/* Display temperature sensor debug information */
		static uint8_t tempDebugStep = 0;
        uint16_t tsCal1 = 0;
        uint16_t tsCal2 = 0; /* kept for adcDebugTemperature signature */
		uint32_t tsData = 0;
		int16_t tempCalc = 0;
		uint16_t paAvgShow = 0;
		
		/* Read temperature debug values */
		adcDebugTemperature(&tsCal1, &tsCal2, &tsData, &tempCalc);
		
		/* Sample PA1 average (raw ADC counts) for debug display */
		adcConfigureChannel(ADC_CHANNEL_FLAME_SENSOR);
		HAL_Delay(2);
		
		{
			float avgValue;
			if (adcReadFlameSensorAverage(&avgValue) == ADC_STATUS_OK) {
				paAvgShow = (uint16_t)avgValue;
			}
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* Display current value based on step */
		if(tempDebugStep == 0)
		{
			/* Display TS_CAL1 (30°C calibration value) */
			dis_value(tsCal1);
		}
		else if(tempDebugStep == 1)
		{
			/* Display current TS_DATA (ADC reading) */
			dis_value(tsData);
		}
		else if(tempDebugStep == 2)
		{
			/* Display calculated temperature (0.1°C units) */
			dis_value(tempCalc);
		}
		else if(tempDebugStep == 3)
		{
			/* Display PA1 average raw ADC counts */
			dis_value(paAvgShow);
		}
		
		/* Wait 3 seconds (with watchdog refresh) */
		for(int i=0; i<30; i++)
		{
			HAL_Delay(100);
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* Clear display (all OFF) for 1 second to indicate switching */
		dis_value(0);
		for(int i=0; i<10; i++)
		{
			HAL_Delay(100);
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* Move to next value */
		tempDebugStep++;
		if(tempDebugStep >= 4)
		{
			tempDebugStep = 0;
		}
		/* ===== END TEMP DEBUG MODE ===== */
#elif ADC_RAW_DEBUG_MODE
		/* ===== ADC RAW DEBUG MODE ===== */
		/* 显示原始ADC数据，帮助排查电压计算问题 */
		static uint8_t adcDebugStep = 0;
		uint32_t vrefintRaw = 0;
		float paAvgRaw = 0.0f;
		float vddaCalc = 0.0f;
		uint32_t voltageMv = 0;
		
		/* 读取原始数据 */
		if (adcReadVrefintSafe(&vrefintRaw) == ADC_STATUS_OK) {
			vddaCalc = adcCalculateVdda(vrefintRaw);
		}
		
		adcConfigureChannel(ADC_CHANNEL_FLAME_SENSOR);
		HAL_Delay(2);
		
		if (adcReadFlameSensorAverage(&paAvgRaw) == ADC_STATUS_OK) {
			float voltageBeforeScale = adcConvertToMillivolts(paAvgRaw, vddaCalc);
			voltageMv = (uint32_t)(voltageBeforeScale * PA1_HARDWARE_SCALE_FACTOR);
		}
		
		/* 循环显示4个值（每个3秒+清屏1秒） */
		if(adcDebugStep == 0) {
			/* 步骤1: VREFINT原始ADC值 (正常1400-1600) */
			dis_value((uint16_t)vrefintRaw);
		}
		else if(adcDebugStep == 1) {
			/* 步骤2: PA1原始ADC平均值 (正常1300-1400) */
			dis_value((uint16_t)paAvgRaw);
		}
		else if(adcDebugStep == 2) {
			/* 步骤3: 计算的VDDA×1000 (如3312=3.312V) */
			dis_value((uint16_t)(vddaCalc * 1000.0f));
		}
		else if(adcDebugStep == 3) {
			/* 步骤4: 校正后的电压mV (应该约2455mV) */
			dis_value((uint16_t)voltageMv);
		}
		
		/* 显示3秒 */
		for(int i=0; i<30; i++) {
			HAL_Delay(100);
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* 清屏1秒 */
		dis_value(0);
		for(int i=0; i<10; i++) {
			HAL_Delay(100);
			HAL_IWDG_Refresh(&hiwdg);
		}
		
		/* 切换到下一个值 */
		adcDebugStep++;
		if(adcDebugStep >= 4) {
			adcDebugStep = 0;
		}
		/* ===== END ADC RAW DEBUG MODE ===== */
#else
        /* Cooperative tasks scheduler */
        uint32_t nowMs = HAL_GetTick();
        taskAdcSample(nowMs);
        taskTempSense(nowMs);
        taskCompensateAndAdapt(nowMs);
        taskControl(nowMs);
        taskDisplay(nowMs);
        taskWatchdog(nowMs);
		
#endif  /* VREFINT_DEBUG_MODE */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}



/* USER CODE BEGIN 4 */

static void taskAdcSample(uint32_t nowMs)
{
    if (nowMs < nextAdcSampleMs) return;
    nextAdcSampleMs = nowMs + TASK_ADC_SAMPLE_PERIOD_MS;

#if ADC_USE_DMA_MODE
    /* V1.0.5: DMA模式采样 */
    if (adcDmaReadFlameSensor(&dmaResult) == ADC_DMA_OK) {
        measValueRawMv = dmaResult.voltageMv;
    }
#else
    /* 旧轮询模式 */
    uint32_t vrefCountCycle;
    float paAvg;
    float vdda;

    if (adcReadVrefintSafe(&vrefCountCycle) != ADC_STATUS_OK) {
        return;
    }

    adcConfigureChannel(ADC_CHANNEL_FLAME_SENSOR);
    HAL_Delay(2);
    
    if (adcReadFlameSensorAverage(&paAvg) != ADC_STATUS_OK) {
        return;
    }

    vdda = adcCalculateVdda(vrefCountCycle);
    measValueRawMv = adcConvertToMillivolts(paAvg, vdda);
    measValueRawMv = measValueRawMv * PA1_HARDWARE_SCALE_FACTOR;
#endif
}

static void taskTempSense(uint32_t nowMs)
{
    if (nowMs < nextTempReadMs) return;
    nextTempReadMs = nowMs + TASK_TEMP_SENSE_PERIOD_MS;

    int16_t tX10;
#if ADC_USE_DMA_MODE
    if (adcDmaReadTemperature(&tX10) == ADC_DMA_OK) {
#else
    if (adcReadTemperatureSafe(&tX10) == ADC_STATUS_OK) {
#endif
        measTempC = (float)tX10 / 10.0f;
        /* IIR 平滑 */
        measTempCFiltered = TEMP_FILTER_BETA * measTempCFiltered + TEMP_FILTER_ALPHA * measTempC;
    }
}

static void taskCompensateAndAdapt(uint32_t nowMs)
{
    if (nowMs < nextCompensateMs) return;
    nextCompensateMs = nowMs + TASK_COMPENSATE_PERIOD_MS;

    float deltaT = measTempCFiltered - TEMP_COMP_REF_C;
#if TEMP_COMP_ENABLE
#if AUTO_K_ENABLE
    measValueCorrMv = measValueRawMv - (kRuntime * deltaT);
#else
    measValueCorrMv = measValueRawMv - (PA1_TEMP_COEFF_MV_PER_C * deltaT);
#endif
    if (measValueCorrMv < 0.0f) measValueCorrMv = 0.0f;

#if AUTO_K_ENABLE
    {
        static float lastRaw = -1.0f;
        static uint32_t stableStart = 0;
        float dV = (lastRaw < 0.0f) ? 0.0f : (measValueRawMv - lastRaw);
        uint8_t stable = (fabsf(dV) < AUTO_K_DVALUE_THRESH_MV);
        if (stable) { if (stableStart == 0) stableStart = nowMs; } else { stableStart = 0; }

        if (!autoKHasTarget) {
            if (AUTO_K_USE_FIXED_REF) {
                autoKTargetMv = AUTO_K_FIXED_REF_MV;
                autoKHasTarget = 1;
            } else if (stable && stableStart && (nowMs - stableStart >= AUTO_K_STABLE_WINDOW_MS)) {
                autoKTargetMv = measValueCorrMv; /* 记首次稳态 */
                autoKHasTarget = 1;
                stableStart = nowMs;
            }
        }

        if (autoKHasTarget && stable && stableStart && (nowMs - stableStart >= AUTO_K_STABLE_WINDOW_MS)) {
            float err = autoKTargetMv - measValueCorrMv;
            float denom = fabsf(deltaT) + AUTO_K_EPS;
            float dk = AUTO_K_ALPHA * (err / denom);
            kRuntime += dk;
            if (kRuntime < AUTO_K_MIN_K) kRuntime = AUTO_K_MIN_K;
            if (kRuntime > AUTO_K_MAX_K) kRuntime = AUTO_K_MAX_K;
            stableStart = nowMs;
        }
        lastRaw = measValueRawMv;
    }
#endif
#else
    measValueCorrMv = measValueRawMv;
#endif
    /* 输出整数显示值 */
    Value = (uint32_t)(measValueCorrMv + 0.5f);
}

/**
  * @brief  更新火焰迟滞状态
  * @param  voltageMv: 当前电压值(mV)
  * @param  nowMs: 当前时间戳
  * @retval 火焰是否存在 (1=存在, 0=熄灭)
  */
static uint8_t updateFlameHysteresis(uint32_t voltageMv, uint32_t nowMs)
{
    /* 迟滞逻辑:
     * - 低于FLAME_THRESH_LOW_MV(2550): 确认有火焰
     * - 高于FLAME_THRESH_HIGH_MV(2750): 确认无火焰
     * - 中间区域: 保持上一状态
     */
    if (voltageMv < FLAME_THRESH_LOW_MV) {
        /* 电压很低，明确有火焰 */
        if (flameHysteresisState != FLAME_HYSTERESIS_PRESENT) {
            flameOnConfirmStartMs = nowMs;
        }
        flameHysteresisState = FLAME_HYSTERESIS_PRESENT;
        return 1;
    }
    else if (voltageMv > FLAME_THRESH_HIGH_MV) {
        /* 电压很高，明确无火焰 */
        flameHysteresisState = FLAME_HYSTERESIS_ABSENT;
        return 0;
    }
    else {
        /* 迟滞区间(2550-2750)，保持上一状态 */
        if (flameHysteresisState == FLAME_HYSTERESIS_PRESENT) {
            return 1;
        } else if (flameHysteresisState == FLAME_HYSTERESIS_ABSENT) {
            return 0;
        } else {
            /* 初始状态，按阈值判断 */
            return (voltageMv < FLAME_THRESHOLD_MV) ? 1 : 0;
        }
    }
}

static void taskControl(uint32_t nowMs)
{
    uint8_t flamePresent;
    
    if (nowMs < nextControlMs) return;
    nextControlMs = nowMs + TASK_CONTROL_PERIOD_MS;

    /* V1.0.5: 使用迟滞逻辑判断火焰状态 */
    flamePresent = updateFlameHysteresis(Value, nowMs);

    if (flamePresent)
    {
        FlameOffCount = 0;
        if (FirstStartCount >= STARTUP_DELAY_MS)
        {
            /* 火焰存在，LED亮，继电器吸合 */
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
        }
    }
    else
    {
        if (FlameOffCount > FLAME_OFF_DELAY_MS)
        {
            /* 火焰熄灭确认，LED灭，继电器断开 */
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
        }
    }
}

static void taskDisplay(uint32_t nowMs)
{
    if (nowMs < nextDisplayMs) return;
    nextDisplayMs = nowMs + TASK_DISPLAY_PERIOD_MS;

#if TEMP_DISPLAY_ENABLE
    /* 每10s进入温度页3s */
    if (!displayInTempPage)
    {
        if (displaySwitchBaseMs == 0) displaySwitchBaseMs = nowMs;
        if (nowMs - displaySwitchBaseMs >= TEMP_DISPLAY_PERIOD_MS)
        {
            displayInTempPage = 1; /* 进入温度页 */
            displaySwitchBaseMs = nowMs; /* 作为温度页起点 */
        }
    }
    else
    {
        if (nowMs - displaySwitchBaseMs >= TEMP_DISPLAY_DURATION_MS)
        {
            displayInTempPage = 0; /* 返回电压页 */
            displaySwitchBaseMs = nowMs; /* 作为新一轮起点 */
        }
    }

    if (displayInTempPage)
    {
        uint16_t tX10 = (uint16_t)(measTempCFiltered * 10.0f + 0.5f);
        dis_value(tX10);
        return;
    }
#endif
    dis_value(Value);
}

static void taskWatchdog(uint32_t nowMs)
{
    if (nowMs < nextWatchdogMs) return;
    nextWatchdogMs = nowMs + TASK_WATCHDOG_PERIOD_MS;
    HAL_IWDG_Refresh(&hiwdg);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

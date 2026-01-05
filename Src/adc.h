/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */
/* Use HAL library definitions for VREFINT_CAL_ADDR and VREFINT_CAL_VREF */
/* They are already defined in stm32g0xx_ll_adc.h */
#define ADC_RESOLUTION      4095  /* 12-bit ADC resolution */

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */
uint32_t adcReadVrefint(void);
float adcGetVdda(void);
void adcDebugVrefint(uint32_t *vrefintAdc, uint16_t *vrefintCal, uint16_t *vddaCalc);
int16_t adcReadTemperature(void);
void adcDebugTemperature(uint16_t *tsCal1, uint16_t *tsCal2, uint32_t *tsData, int16_t *tempCalc);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */


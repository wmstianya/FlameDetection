/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : MCU2 D380 SPI Temperature Bridge — main header
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Top-level header for the MCU2 STM32G070 sub-chip firmware.
  * Pulls in STM32G0xx HAL and exposes Error_Handler().
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Called on unrecoverable errors. Halts execution in debug builds;
  *         triggers a watchdog reset in release builds.
  * @retval None — does not return.
  */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ END OF FILE *****************************/

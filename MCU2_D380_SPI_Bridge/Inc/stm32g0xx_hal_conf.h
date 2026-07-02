/**
  ******************************************************************************
  * @file           : stm32g0xx_hal_conf.h
  * @brief          : HAL configuration for MCU2 D380 SPI Temperature Bridge
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Selects only the HAL modules required by MCU2 firmware:
  *   - HAL core (GPIO, CORTEX, RCC)
  *   - SPI (SPI1 master + SPI2 slave)
  *   - IWDG
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#ifndef __STM32G0XX_HAL_CONF_H
#define __STM32G0XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   Module Selection
   ============================================================================ */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED

/* ============================================================================
   Oscillator Values (HSE not used on MCU2 — HSI only)
   ============================================================================ */
#if !defined(HSE_VALUE)
#define HSE_VALUE               8000000u  /**< External oscillator (Hz) */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT     100u      /**< HSE startup timeout (ms) */
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE               16000000u /**< Internal oscillator (Hz) */
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE               32000u    /**< Internal low-speed oscillator (Hz) */
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE               32768u    /**< External 32 kHz crystal (Hz) */
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT     5000u     /**< LSE startup timeout (ms) */
#endif

/* ============================================================================
   System Configuration
   ============================================================================ */
#define VDD_VALUE               3300u     /**< VDD supply voltage (mV) */
#define TICK_INT_PRIORITY       0u        /**< SysTick interrupt priority */
#define USE_RTOS                0u        /**< No RTOS */
#define PREFETCH_ENABLE         1u        /**< Flash prefetch enabled */
#define INSTRUCTION_CACHE_ENABLE 1u

/* ============================================================================
   HAL Include Chain
   ============================================================================ */
#include "stm32g0xx_hal_def.h"

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32g0xx_hal_cortex.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32g0xx_hal_dma.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32g0xx_hal_flash.h"
#include "stm32g0xx_hal_flash_ex.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32g0xx_hal_gpio.h"
#include "stm32g0xx_hal_gpio_ex.h"
#endif

#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32g0xx_hal_iwdg.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32g0xx_hal_pwr.h"
#include "stm32g0xx_hal_pwr_ex.h"
#endif

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_hal_rcc_ex.h"
#endif

#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32g0xx_hal_spi.h"
#include "stm32g0xx_hal_spi_ex.h"
#endif

/* ============================================================================
   Assert Support
   ============================================================================ */
#ifdef USE_FULL_ASSERT
#define assert_param(expr) \
    ((expr) ? (void)0u : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0u)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32G0XX_HAL_CONF_H */

/************************ END OF FILE *****************************/

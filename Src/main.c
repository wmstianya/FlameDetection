/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Flame detection main entry: clock, ADC, display, and
  *                   IWDG init, then hand off control to the flame module
  *                   inside an IWDG-fed forever loop.
  *
  * @author         2026-05-08
  * @date           2026-05-08
  * @version        V1.2.0
  *
  * @revision
  *   V1.0.0  2024       Initial version (STM32CubeMX generated)
  *   V1.0.1  2025-02-19 Add 2 s startup blanking; 0.5 s flame-off delay
  *   V1.0.2  2025-03-31 Threshold 2500 -> 2680 mV; flame-off 0.5 -> 1.5 s
  *   V1.1.0  2026-05-08 Refactor: overflow fix, naming, magic numbers,
  *                       function split, SysTick moved to stm32g0xx_it.c
  *   V1.2.0  2026-05-08 Industrial hardening: VREFINT-calibrated VDDA,
  *                       trimmed-mean filter, fault-detection gate (wet
  *                       probe / open / non-physical signal protection),
  *                       adaptive baseline, Schmitt thresholds, dedicated
  *                       flame.c state machine module
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tm1650.h"
#include "flame.h"

/* Private variables ---------------------------------------------------------*/
static IWDG_HandleTypeDef hiwdg;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_IWDG_Init(void);

/* ---------------------------------------------------------------------------*/
/*              Application entry point                                       */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  The application entry point.
  * @retval int (never returns)
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_ADC1_Init();      /* also brings up DMA1_CH1 + TIM6 1 kHz */
    tm1650Init();
    flameInit();
    MX_IWDG_Init();

    adcStartFrame();     /* fire the very first DMA frame */

    while (1)
    {
        HAL_IWDG_Refresh(&hiwdg);
        flameProcess();  /* internally __WFI()s while DMA fills the frame */
    }
}

/* ---------------------------------------------------------------------------*/
/*              System Clock Configuration                                    */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  Configure HSE + PLL -> 64 MHz SYSCLK.
  *         HSE 8 MHz * PLLN 16 / PLLR 2 = 64 MHz.
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI
                                     | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState       = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN       = 16;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLR       = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ---------------------------------------------------------------------------*/
/*              IWDG                                                          */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  IWDG init -- prescaler 4, reload 4095 => ~0.5 s timeout @ 32 kHz LSI.
  *         Refresh from main loop once per flame cycle (~128 ms).
  * @retval None
  */
static void MX_IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
    hiwdg.Init.Window    = IWDG_WINDOW_VALUE;
    hiwdg.Init.Reload    = IWDG_RELOAD_VALUE;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ---------------------------------------------------------------------------*/
/*              Error / assert handlers                                       */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  Fatal error handler.  Disables interrupts and blocks; the IWDG
  *         (already running) will reset the MCU within ~0.5 s, providing
  *         a fail-safe path even if Error_Handler() is hit before IWDG init.
  * @retval Never returns
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
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
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

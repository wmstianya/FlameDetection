/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : MCU2 D380 SPI Temperature Bridge — main entry point
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * Firmware for the STM32G070 sub-chip (MCU2) in the D380 board.
  * Responsibilities:
  *   1. Read temperature from the D380 SPI temperature sensor via SPI1 master.
  *   2. Expose the temperature to MCU1 via SPI2 slave interface.
  *   3. Toggle a status LED to indicate liveness.
  *   4. Maintain an IWDG watchdog with a ~4 s timeout.
  *
  * Cooperative scheduler periods (all in ms — configured in bridgeConfig.h):
  *   TASK_SENSOR_READ_PERIOD_MS  1000   D380 conversion cycle
  *   TASK_SLAVE_UPDATE_PERIOD_MS  100   Refresh SPI slave shadow frame
  *   TASK_STATUS_LED_PERIOD_MS    500   LED heartbeat toggle
  *   TASK_WATCHDOG_PERIOD_MS      200   Watchdog kick
  *
  * System clock: HSI 16 MHz, PLL → 64 MHz SYSCLK (same as MCU1).
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "Src/config/bridgeConfigTypes.h"
#include "Src/drivers/spiMaster.h"
#include "Src/drivers/spiSlave.h"
#include "Src/drivers/tempSensorD380.h"
#include "Src/application/tempBridge.h"

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;

static BridgeConfig bridgeCfg;
static TempBridge   tempBridgeCtx;

/* Cooperative scheduler timestamps */
static uint32_t nextSensorMs    = 0u;
static uint32_t nextSlaveMs     = 0u;
static uint32_t nextLedMs       = 0u;
static uint32_t nextWatchdogMs  = 0u;

/* Private function prototypes -----------------------------------------------*/
static void systemClockConfig(void);
static void configureOscillators(void);
static void configureBusDividers(void);
static void iwdgInit(void);
static void statusLedInit(void);
static void systemInit(void);
static void taskStatusLed(uint32_t nowMs);
static void taskWatchdog(uint32_t nowMs);

/* ============================================================================
   Peripheral Initialisation
   ============================================================================ */

/**
  * @brief  Configure oscillators and PLL source for 64 MHz SYSCLK.
  * @retval None
  */
static void configureOscillators(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI
                                          | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv              = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState            = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN            = 8;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief  Select PLL as SYSCLK source and configure AHB/APB dividers.
  * @retval None
  */
static void configureBusDividers(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  Configure PLL: HSI 16 MHz → PLL → 64 MHz SYSCLK.
  */
static void systemClockConfig(void)
{
    configureOscillators();
    configureBusDividers();
}

/**
  * @brief  Initialise IWDG with ~4 s timeout (LSI / prescaler_32 / 4000).
  */
static void iwdgInit(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_VALUE;
    hiwdg.Init.Window    = IWDG_RELOAD_VALUE;
    hiwdg.Init.Reload    = IWDG_RELOAD_VALUE;

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  Initialise the status LED GPIO (PC6, push-pull output).
  */
static void statusLedInit(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpioInit.Pin   = STATUS_LED_PIN;
    gpioInit.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull  = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(STATUS_LED_PORT, &gpioInit);

    /* Start with LED off */
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);
}

/* ============================================================================
   Cooperative Scheduler Tasks
   ============================================================================ */

/**
  * @brief  Toggle the status LED at the configured heartbeat rate.
  * @param  nowMs  Current HAL tick.
  */
static void taskStatusLed(uint32_t nowMs)
{
    if (nowMs < nextLedMs) { return; }
    nextLedMs = nowMs + bridgeCfg.taskStatusLedMs;

    HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
}

/**
  * @brief  Kick the IWDG periodically.
  * @param  nowMs  Current HAL tick.
  */
static void taskWatchdog(uint32_t nowMs)
{
    if (nowMs < nextWatchdogMs) { return; }
    nextWatchdogMs = nowMs + bridgeCfg.taskWatchdogMs;

    HAL_IWDG_Refresh(&hiwdg);
}

/**
  * @brief  Sensor-read cooperative task wrapper.
  * @param  nowMs  Current HAL tick.
  */
static void taskSensor(uint32_t nowMs)
{
    if (nowMs < nextSensorMs) { return; }
    nextSensorMs = nowMs + bridgeCfg.taskSensorPeriodMs;

    tempBridgeTaskSensor(&tempBridgeCtx, nowMs);
}

/**
  * @brief  SPI slave service cooperative task wrapper.
  * @param  nowMs  Current HAL tick.
  */
static void taskSlaveService(uint32_t nowMs)
{
    if (nowMs < nextSlaveMs) { return; }
    nextSlaveMs = nowMs + bridgeCfg.taskSlaveUpdateMs;

    tempBridgeTaskSlave(&tempBridgeCtx, nowMs);
}

/* ============================================================================
   Entry Point
   ============================================================================ */

/**
  * @brief  Initialise all peripherals and application contexts.
  * @retval None
  */
static void systemInit(void)
{
    HAL_Init();
    systemClockConfig();
    spiMasterInit();
    spiSlaveInit();
    statusLedInit();
    iwdgInit();
    bridgeConfigInit(&bridgeCfg);
    tempBridgeInit(&tempBridgeCtx, &bridgeCfg);
}

/**
  * @brief  Application entry point.
  * @retval int (never returns)
  */
int main(void)
{
    systemInit();

    while (1) {
        uint32_t nowMs = HAL_GetTick();
        taskWatchdog(nowMs);
        taskSensor(nowMs);
        taskSlaveService(nowMs);
        taskStatusLed(nowMs);
    }
}

/* ============================================================================
   Interrupt Handlers
   ============================================================================ */

/**
  * @brief  SysTick interrupt handler — calls HAL_IncTick().
  */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ============================================================================
   Error Handler
   ============================================================================ */

/**
  * @brief  Unrecoverable error handler.
  * @note   Disables interrupts and spins; the IWDG will force a reset after
  *         its timeout window expires.
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* Spin and let IWDG reset the system */
    }
}

/************************ END OF FILE *****************************/

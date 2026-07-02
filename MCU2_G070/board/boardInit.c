/**
 * @file    boardInit.c
 * @brief   HSE 64 MHz, indicator GPIO, WDI feed and LED heartbeat
 */
#include "boardInit.h"
#include "../config/mcu2Types.h"
#include "../config/mcu2Pins.h"
#include "../Inc/mcu2Main.h"

#define WDI_TOGGLE_MS       500U
#define RUN_LED_PERIOD_MS   1000U
#define COM_LED_PULSE_MS    80U

static volatile uint32_t gMsTick = 0U;
static uint32_t gLastWdiMs = 0U;
static uint32_t gLastRunMs = 0U;
static uint32_t gComOffMs = 0U;
static uint8 gWdiLevel = 0U;
static uint8 gRunLevel = 0U;

uint32_t boardGetMsTick(void)
{
    return gMsTick;
}

static void boardGpioClockEnable(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
}

static void boardOutputInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = MCU2_WDI_PIN;
    HAL_GPIO_Init(MCU2_WDI_PORT, &gpio);
    gpio.Pin = MCU2_COM_LED_PIN;
    HAL_GPIO_Init(MCU2_COM_LED_PORT, &gpio);
    gpio.Pin = MCU2_RUN_LED_PIN;
    HAL_GPIO_Init(MCU2_RUN_LED_PORT, &gpio);

    HAL_GPIO_WritePin(MCU2_WDI_PORT, MCU2_WDI_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MCU2_COM_LED_PORT, MCU2_COM_LED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MCU2_RUN_LED_PORT, MCU2_RUN_LED_PIN, GPIO_PIN_RESET);
}

static void boardClockConfig(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = RCC_PLLM_DIV1;
    osc.PLL.PLLN = 16;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

void boardInit(void)
{
    HAL_Init();
    boardClockConfig();
    boardGpioClockEnable();
    boardOutputInit();
    gMsTick = 0U;
}

void boardSysTickInc(void)
{
    uint32_t now = ++gMsTick;
    if ((now - gLastWdiMs) >= WDI_TOGGLE_MS)
    {
        gLastWdiMs = now;
        gWdiLevel ^= 1U;
        HAL_GPIO_WritePin(MCU2_WDI_PORT, MCU2_WDI_PIN,
                          gWdiLevel ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    if (gComOffMs != 0U && now >= gComOffMs)
    {
        HAL_GPIO_WritePin(MCU2_COM_LED_PORT, MCU2_COM_LED_PIN, GPIO_PIN_RESET);
        gComOffMs = 0U;
    }
}

void wdiFeedToggle(void)
{
    /* WDI toggled from SysTick; hook for future use */
}

void ledComPulse(void)
{
    gComOffMs = gMsTick + COM_LED_PULSE_MS;
    HAL_GPIO_WritePin(MCU2_COM_LED_PORT, MCU2_COM_LED_PIN, GPIO_PIN_SET);
}

void runLedHeartbeat(void)
{
    uint32_t now = gMsTick;
    if ((now - gLastRunMs) < RUN_LED_PERIOD_MS)
        return;
    gLastRunMs = now;
    gRunLevel ^= 1U;
    HAL_GPIO_WritePin(MCU2_RUN_LED_PORT, MCU2_RUN_LED_PIN,
                      gRunLevel ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

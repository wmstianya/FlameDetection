/**
  ******************************************************************************
  * @file    tm1650.c
  * @brief   TM1650 4-digit 7-segment LED driver over software IIC (PD1=CLK,
  *          PD2=SDA).  Also initialises LED indicator (PA10) and relay (PA11).
  * @author  Refactored 2026-05-08
  * @date    2026-05-08
  * @version V1.1.0
  *
  * @revision
  *   V1.0.0  2024  Initial version
  *   V1.1.0  2026-05-08  camelCase rename, volatile delay fix, IIC ACK early
  *           return, brightness sent once at init, function split <=20 lines,
  *           remove dead code Send_To_TM1650
  ******************************************************************************
  */
#include "tm1650.h"

/* ---- Hardware pin mapping (IIC bit-bang) --------------------------------- */
#define IIC_CLK_PORT            GPIOD
#define IIC_CLK_PIN             GPIO_PIN_1
#define IIC_SDA_PORT            GPIOD
#define IIC_SDA_PIN             GPIO_PIN_2

/* ---- Timing -------------------------------------------------------------- */
#define DELAY_US_FACTOR         64U   /* SYSCLK = 64 MHz -> ~1 us per unit */

/* ---- TM1650 protocol ----------------------------------------------------- */
#define TM1650_CMD_ADDR         0x48U
#define TM1650_BRIGHTNESS_CMD   0x71U /* 8/16 duty, 8-segment mode, ON */
#define TM1650_DIGIT_COUNT      4U
#define IIC_ACK_TIMEOUT         100U

/* 7-segment encoding for digits 0-9 (common-cathode, active-high segments) */
static const uint8_t SEGMENT_MAP[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

/* Letter glyphs used by tm1650ShowFaultCode */
#define SEG_LETTER_E            0x79U  /* 'E' */
#define SEG_LETTER_R            0x50U  /* lowercase 'r' */

static const uint8_t DIGIT_ADDR[TM1650_DIGIT_COUNT] = {
    0x68U, 0x6AU, 0x6CU, 0x6EU
};

/* ---- IIC bit-bang primitives (static, module-internal) -------------------- */

static void delayUs(uint16_t us)
{
    if (us == 0U) { return; }
    volatile uint32_t cnt = (uint32_t)us * DELAY_US_FACTOR;
    while (cnt > 0U) { cnt--; }
}

static void iicClkSet(uint8_t v)
{
    HAL_GPIO_WritePin(IIC_CLK_PORT, IIC_CLK_PIN, (GPIO_PinState)v);
}

static void iicSdaSet(uint8_t v)
{
    HAL_GPIO_WritePin(IIC_SDA_PORT, IIC_SDA_PIN, (GPIO_PinState)v);
}

static void iicStart(void)
{
    iicClkSet(1);
    delayUs(1);
    iicSdaSet(1);
    delayUs(1);
    iicSdaSet(0);
    delayUs(1);
    iicClkSet(0);
    delayUs(1);
}

static void iicStop(void)
{
    iicSdaSet(0);
    delayUs(1);
    iicClkSet(1);
    delayUs(1);
    iicSdaSet(1);
    delayUs(1);
}

/**
  * @brief  Wait for ACK from TM1650 after a byte transfer.
  * @retval 1 ACK received, 0 timeout (NACK)
  */
static uint8_t iicWaitAck(void)
{
    uint8_t timeout = 0U;

    iicClkSet(0);
    iicSdaSet(1);            /* release SDA before CLK rises (IIC spec) */
    delayUs(1);
    iicClkSet(1);
    delayUs(1);

    while (HAL_GPIO_ReadPin(IIC_SDA_PORT, IIC_SDA_PIN) != GPIO_PIN_RESET)
    {
        timeout++;
        delayUs(1);
        if (timeout > IIC_ACK_TIMEOUT)
        {
            iicClkSet(0);    /* leave CLK low on timeout for clean bus state */
            return 0U;
        }
    }

    iicClkSet(0);
    return 1U;
}

static void iicSendByte(uint8_t data)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        iicClkSet(0);
        iicSdaSet((data & 0x80U) ? 1U : 0U);
        delayUs(1);
        iicClkSet(1);
        data <<= 1U;
    }
}

/* ---- GPIO sub-initialisers ----------------------------------------------- */

static void tm1650IicGpioInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(IIC_CLK_PORT, IIC_CLK_PIN | IIC_SDA_PIN, GPIO_PIN_RESET);

    gpio.Pin   = IIC_CLK_PIN | IIC_SDA_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(IIC_CLK_PORT, &gpio);

    HAL_GPIO_WritePin(IIC_CLK_PORT, IIC_CLK_PIN | IIC_SDA_PIN, GPIO_PIN_SET);
}

static void ledRelayGpioInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_PORT, LED_PIN | RELAY_PIN, GPIO_PIN_RESET);

    gpio.Pin   = LED_PIN | RELAY_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);     /* LED OFF  */
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET); /* Relay OFF */
}

/**
  * @brief  Send brightness / display-on command to TM1650 (once at boot).
  */
static void tm1650SetBrightness(void)
{
    iicStart();
    iicSendByte(TM1650_CMD_ADDR);
    if (iicWaitAck() == 0U) { iicStop(); return; }
    iicSendByte(TM1650_BRIGHTNESS_CMD);
    if (iicWaitAck() == 0U) { iicStop(); return; }
    iicStop();
}

/* ---- Public API ---------------------------------------------------------- */

void tm1650Init(void)
{
    tm1650IicGpioInit();
    ledRelayGpioInit();
    tm1650SetBrightness();
}

/**
  * @brief  Write a segment pattern to one digit position via IIC.
  * @param  addr   TM1650 digit register address (0x68/0x6A/0x6C/0x6E)
  * @param  value  7-segment pattern byte
  * @retval 1 on success, 0 on IIC ACK failure
  */
uint8_t tm1650WriteDigit(uint8_t addr, uint8_t value)
{
    iicStart();
    iicSendByte(addr);
    if (iicWaitAck() == 0U) { iicStop(); return 0U; }
    iicSendByte(value);
    if (iicWaitAck() == 0U) { iicStop(); return 0U; }
    iicStop();
    return 1U;
}

/**
  * @brief  Display a 0-9999 integer with leading-zero blanking.
  * @param  data  Value to display (0 .. 9999)
  * @retval None
  */
void tm1650ShowValue(uint16_t data)
{
    uint8_t digits[TM1650_DIGIT_COUNT];
    uint8_t leadingZero = 1U;

    digits[0] = (uint8_t)(data / 1000U);
    digits[1] = (uint8_t)((data % 1000U) / 100U);
    digits[2] = (uint8_t)((data % 100U) / 10U);
    digits[3] = (uint8_t)(data % 10U);

    for (uint8_t i = 0U; i < TM1650_DIGIT_COUNT; i++)
    {
        uint8_t isLastDigit = (i == TM1650_DIGIT_COUNT - 1U);
        if (!isLastDigit && (digits[i] == 0U) && leadingZero)
        {
            tm1650WriteDigit(DIGIT_ADDR[i], 0x00U);
        }
        else
        {
            leadingZero = 0U;
            tm1650WriteDigit(DIGIT_ADDR[i], SEGMENT_MAP[digits[i]]);
        }
    }
}

/**
  * @brief  Display "ErrN" pattern: E r r digit
  * @param  code  Fault digit 0..9 (truncated by modulo)
  * @retval None
  */
void tm1650ShowFaultCode(uint8_t code)
{
    uint8_t digit = (uint8_t)(code % 10U);

    tm1650WriteDigit(DIGIT_ADDR[0], SEG_LETTER_E);
    tm1650WriteDigit(DIGIT_ADDR[1], SEG_LETTER_R);
    tm1650WriteDigit(DIGIT_ADDR[2], SEG_LETTER_R);
    tm1650WriteDigit(DIGIT_ADDR[3], SEGMENT_MAP[digit]);
}

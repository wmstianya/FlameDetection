/**
 * @file    ads1220Port.c
 * @brief   ADS1220 on PB3–PB7; temperature via tempCalib
 */
#include "ads1220Port.h"
#include "../config/mcu2Pins.h"
#include "../protocol/hostProtocol.h"
#include "../utils/softSpiBitbang.h"
#include "../utils/tempCalib.h"

#define ADS1220_CMD_RDATA       0x10U
#define ADS1220_CMD_WREG        0x40U
#define ADS1220_CMD_RESET       0x06U
#define ADS1220_CMD_SYNC        0x08U

#define ADS1220_MUX_1_0         0x30U
#define ADS1220_GAIN_16         0x08U
#define ADS1220_CC              0x04U
#define ADS1220_IDAC_500        0x10U
#define ADS1220_IDAC1_AIN2      0x20U
#define ADS1220_IDAC2_AIN3      0x40U

static uint16 gLastTempC = 300U;

static void ads1220WriteReg(uint8_t addr, uint8_t value)
{
    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte((uint8_t)(ADS1220_CMD_WREG | ((addr << 2) & 0x0CU)));
    softSpiBitbangSendByte(value);
    softSpiBitbangCsSet(0U);
}

static uint8 ads1220DrdyReady(void)
{
    return (HAL_GPIO_ReadPin(MCU2_ADS_DRDY_PORT, MCU2_ADS_DRDY_PIN) == GPIO_PIN_RESET)
               ? 1U
               : 0U;
}

void ads1220PortInit(void)
{
    softSpiBitbangInit();
    softSpiBitbangCsSet(0U);
}

void ads1220PortConfig(void)
{
    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte(ADS1220_CMD_RESET);
    softSpiBitbangCsSet(0U);
    HAL_Delay(2U);

    ads1220WriteReg(0U, (uint8_t)(ADS1220_MUX_1_0 | ADS1220_GAIN_16));
    ads1220WriteReg(1U, ADS1220_CC);
    ads1220WriteReg(2U, (uint8_t)(ADS1220_IDAC_500 | 0x50U));
    ads1220WriteReg(3U, (uint8_t)(ADS1220_IDAC1_AIN2 | ADS1220_IDAC2_AIN3));

    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte(ADS1220_CMD_SYNC);
    softSpiBitbangCsSet(0U);
}

uint8 ads1220PortTryReadRaw(int32_t *rawOut)
{
    int32_t data;
    if (rawOut == NULL)
        return ADS1220_PORT_ERROR;
    if (!ads1220DrdyReady())
        return ADS1220_PORT_NOT_READY;

    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte(ADS1220_CMD_RDATA);
    data = (int32_t)softSpiBitbangRecvByte();
    data = (data << 8) | softSpiBitbangRecvByte();
    data = (data << 8) | softSpiBitbangRecvByte();
    softSpiBitbangCsSet(0U);
    if (data & 0x800000)
        data |= (int32_t)0xFF000000;
    *rawOut = data;
    return ADS1220_PORT_OK;
}

uint16 ads1220PortReadTempC(uint8 *statOut)
{
    int32_t raw = 0;
    uint8 ok = 0U;
    uint8 portStat;
    uint16 tempC;

    portStat = ads1220PortTryReadRaw(&raw);
    if (portStat == ADS1220_PORT_OK)
    {
        tempC = tempCalibRawToTempC(raw, &ok);
        if (ok != 0U)
        {
            gLastTempC = tempC;
            if (statOut != NULL)
                *statOut = HOST_STAT_OK;
            return tempC;
        }
        if (statOut != NULL)
            *statOut = HOST_STAT_FAULT;
        return HOST_TEMP_CLAMP_MAX;
    }
    if (portStat == ADS1220_PORT_NOT_READY)
    {
        if (statOut != NULL)
            *statOut = HOST_STAT_OK;
        return gLastTempC;
    }
    if (statOut != NULL)
        *statOut = HOST_STAT_FAULT;
    return HOST_TEMP_CLAMP_MAX;
}

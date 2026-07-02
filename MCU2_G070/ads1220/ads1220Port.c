/**
 * @file    ads1220Port.c
 * @brief   ADS1220 (U27) bit-bang port on PB3-PB7; furnace temperature acquisition.
 * @details Register configuration mirrors the D380 master ADS1220Config(); the
 *          fault/staleness policy is delegated to the pure ads1220Reading module.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  A1 dead-sensor detection; C1 Reg2 IDAC current fix; de-magic.
 */
#include "ads1220Port.h"
#include "ads1220Reading.h"
#include "../config/mcu2Pins.h"
#include "../protocol/hostProtocol.h"
#include "../utils/softSpiBitbang.h"
#include "../utils/tempCalib.h"
#include "../Inc/mcu2Main.h"    /* boardGetMsTick() */

/* ADS1220 command set (datasheet 8.5.3). */
#define ADS1220_CMD_RDATA           0x10U
#define ADS1220_CMD_WREG            0x40U
#define ADS1220_CMD_RESET           0x06U
#define ADS1220_CMD_SYNC            0x08U

/* WREG opcode = 0100 rrnn : rr = start register, nn = (register count - 1). */
#define ADS1220_WREG_ADDR_SHIFT     2U
#define ADS1220_WREG_ADDR_MASK      0x0CU

/* Configuration register indices. */
#define ADS1220_REG_CONFIG0         0U
#define ADS1220_REG_CONFIG1         1U
#define ADS1220_REG_CONFIG2         2U
#define ADS1220_REG_CONFIG3         3U

/* Reg0 fields: input MUX and PGA gain (values carried over from D380 master). */
#define ADS1220_MUX_1_0             0x30U
#define ADS1220_GAIN_16             0x08U

/* Reg1 field: continuous conversion mode (CM = 1). */
#define ADS1220_CC                  0x04U

/*
 * Reg2 fields (datasheet Table: [7:6]=VREF, [5:4]=50/60Hz FIR, [2:0]=IDAC).
 * C1 FIX: the IDAC current lives in bits [2:0]; 500 uA = 0b101 = 0x05. The
 * previous value 0x10 landed in the [5:4] filter field, so the emitted Reg2
 * (0x50) had IDAC = 000 (OFF) and the RTD excitation never flowed. The value
 * now emits 0x55 to match the documented intent "external VREF, 50/60Hz, IDAC
 * 500 uA". VERIFY the master's ADS1220_IDAC_500 macro is likewise 0x05.
 */
#define ADS1220_VREF_EXTERNAL       0x40U   /* external REFP0/REFN0            */
#define ADS1220_FILTER_50_60HZ      0x10U   /* simultaneous 50/60Hz rejection  */
#define ADS1220_IDAC_500            0x05U   /* IDAC current = 500 uA (bits[2:0])*/

/*
 * Reg3 fields: IDAC1/IDAC2 output routing (kept byte-identical to the master).
 * NOTE: these macro names do not match the datasheet I1MUX/I2MUX bit fields;
 * left unchanged to preserve parity with the master's known configuration.
 * VERIFY against the master ADS1220.c before altering the routing.
 */
#define ADS1220_IDAC1_AIN2          0x20U
#define ADS1220_IDAC2_AIN3          0x40U

/* 24-bit two's-complement sign handling for the conversion result. */
#define ADS1220_SIGN_BIT_24         0x00800000
#define ADS1220_SIGN_EXTEND_24      ((int32_t)0xFF000000)

/* Provisional temperature reported during the power-on warm-up window. */
#define ADS1220_SEED_TEMP_C         300U

/* Reset settle time before register configuration (datasheet t_RST). */
#define ADS1220_RESET_SETTLE_MS     2U

static Ads1220Reading gReading;

/**
 * @brief  Write one ADS1220 configuration register.
 * @param  addr  Register index (ADS1220_REG_CONFIGx).
 * @param  value Register contents to write.
 * @return None.
 */
static void ads1220WriteReg(uint8_t addr, uint8_t value)
{
    uint8_t opcode = (uint8_t)(ADS1220_CMD_WREG |
                     ((addr << ADS1220_WREG_ADDR_SHIFT) & ADS1220_WREG_ADDR_MASK));
    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte(opcode);
    softSpiBitbangSendByte(value);
    softSpiBitbangCsSet(0U);
}

/**
 * @brief  Test the DRDY line (active low) for a completed conversion.
 * @return 1 when a fresh sample is ready, 0 otherwise.
 */
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
    ads1220ReadingInit(&gReading, ADS1220_SEED_TEMP_C);
}

void ads1220PortConfig(void)
{
    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte(ADS1220_CMD_RESET);
    softSpiBitbangCsSet(0U);
    HAL_Delay(ADS1220_RESET_SETTLE_MS);

    ads1220WriteReg(ADS1220_REG_CONFIG0, (uint8_t)(ADS1220_MUX_1_0 | ADS1220_GAIN_16));
    ads1220WriteReg(ADS1220_REG_CONFIG1, ADS1220_CC);
    ads1220WriteReg(ADS1220_REG_CONFIG2,
                    (uint8_t)(ADS1220_VREF_EXTERNAL | ADS1220_FILTER_50_60HZ |
                              ADS1220_IDAC_500));
    ads1220WriteReg(ADS1220_REG_CONFIG3,
                    (uint8_t)(ADS1220_IDAC1_AIN2 | ADS1220_IDAC2_AIN3));

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
    if ((data & ADS1220_SIGN_BIT_24) != 0)
        data |= ADS1220_SIGN_EXTEND_24;
    *rawOut = data;
    return ADS1220_PORT_OK;
}

uint16 ads1220PortReadTempC(uint8 *statOut)
{
    int32_t raw = 0;
    uint8 convValid = 0U;
    uint16 convTempC = 0U;
    uint8 portStat;

    portStat = ads1220PortTryReadRaw(&raw);
    if (portStat == ADS1220_PORT_OK)
        convTempC = tempCalibRawToTempC(raw, &convValid);

    return ads1220ReadingResolve(&gReading, portStat, convTempC, convValid,
                                 boardGetMsTick(), statOut);
}

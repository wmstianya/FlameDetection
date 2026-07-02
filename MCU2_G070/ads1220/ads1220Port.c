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
#include "../utils/tempFilter.h"
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

/*
 * Register values are byte-aligned with the D380 master ADS1220Config()
 * (docs OVERVIEW.md 3.2/4): Reg0=0x68, Reg1=0x04, Reg2=0x55, Reg3=0x70.
 * The compile-time checks at the end of this block lock that alignment.
 */

/* Reg0 = 0x68: MUX AIN1-AIN0 (bits[7:4]=0110=0x60) | PGA gain x16 (bits[3:1]=100). */
#define ADS1220_MUX_AIN1_AIN0       0x60U
#define ADS1220_GAIN_16             0x08U

/* Reg1 = 0x04: continuous conversion (CM=1); DR=20 SPS, Normal mode (defaults). */
#define ADS1220_CC                  0x04U

/*
 * Reg2 = 0x55: [7:6]=VREF, [5:4]=50/60Hz FIR, [2:0]=IDAC current. The IDAC
 * current is 0b101=0x05 (500 uA) in bits[2:0] -- NOT 0x10, which is the 50/60Hz
 * field. The RTD probe requires 500 uA excitation; must not be left OFF.
 */
#define ADS1220_VREF_EXTERNAL       0x40U   /* external REFP0/REFN0            */
#define ADS1220_FILTER_50_60HZ      0x10U   /* simultaneous 50/60Hz rejection  */
#define ADS1220_IDAC_500            0x05U   /* IDAC current = 500 uA (bits[2:0])*/

/*
 * Reg3 = 0x70: [7:5]=I1MUX, [4:2]=I2MUX. Route IDAC1 -> AIN2 (011 in [7:5]=0x60)
 * and IDAC2 -> AIN3 (100 in [4:2]=0x10). Both sources must be routed; the older
 * 0x20/0x40 literals put both codes in the I1MUX field and left IDAC2 disabled.
 */
#define ADS1220_IDAC1_AIN2          0x60U
#define ADS1220_IDAC2_AIN3          0x10U

/* Composed configuration bytes (must equal the master's 0x68/0x04/0x55/0x70). */
#define ADS1220_REG0_VALUE  ((uint8_t)(ADS1220_MUX_AIN1_AIN0 | ADS1220_GAIN_16))
#define ADS1220_REG1_VALUE  ((uint8_t)(ADS1220_CC))
#define ADS1220_REG2_VALUE  ((uint8_t)(ADS1220_VREF_EXTERNAL | ADS1220_FILTER_50_60HZ | \
                                       ADS1220_IDAC_500))
#define ADS1220_REG3_VALUE  ((uint8_t)(ADS1220_IDAC1_AIN2 | ADS1220_IDAC2_AIN3))

/* Fail the build if the configuration drifts from the master's ADS1220 bytes. */
typedef char ads1220Reg0Aligned[(ADS1220_REG0_VALUE == 0x68U) ? 1 : -1];
typedef char ads1220Reg1Aligned[(ADS1220_REG1_VALUE == 0x04U) ? 1 : -1];
typedef char ads1220Reg2Aligned[(ADS1220_REG2_VALUE == 0x55U) ? 1 : -1];
typedef char ads1220Reg3Aligned[(ADS1220_REG3_VALUE == 0x70U) ? 1 : -1];

/* 24-bit two's-complement sign handling for the conversion result. */
#define ADS1220_SIGN_BIT_24         0x00800000
#define ADS1220_SIGN_EXTEND_24      ((int32_t)0xFF000000)

/* Provisional temperature reported during the power-on warm-up window. */
#define ADS1220_SEED_TEMP_C         300U

/* Reset settle time before register configuration (datasheet t_RST). */
#define ADS1220_RESET_SETTLE_MS     2U

static Ads1220Reading gReading;
static TempFilter gTempFilter;

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
    tempFilterInit(&gTempFilter);
}

void ads1220PortConfig(void)
{
    softSpiBitbangCsSet(1U);
    softSpiBitbangSendByte(ADS1220_CMD_RESET);
    softSpiBitbangCsSet(0U);
    HAL_Delay(ADS1220_RESET_SETTLE_MS);

    ads1220WriteReg(ADS1220_REG_CONFIG0, ADS1220_REG0_VALUE);
    ads1220WriteReg(ADS1220_REG_CONFIG1, ADS1220_REG1_VALUE);
    ads1220WriteReg(ADS1220_REG_CONFIG2, ADS1220_REG2_VALUE);
    ads1220WriteReg(ADS1220_REG_CONFIG3, ADS1220_REG3_VALUE);

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
    {
        /* Acquisition path (~20 SPS): filter the 0.1 deg C table value before
         * the trim/reduction, then reduce to whole degrees. The filter is only
         * advanced on a fresh sample, so it tracks the ADS1220 conversion rate
         * and is independent of the 1 Hz host SPI polling. */
        uint16 rawTenthC = tempCalibRawToTenthC(raw);
        uint16 filteredTenthC = tempFilterPush(&gTempFilter, rawTenthC);
        convTempC = tempCalibTenthToTempC(filteredTenthC, &convValid);
    }

    return ads1220ReadingResolve(&gReading, portStat, convTempC, convValid,
                                 boardGetMsTick(), statOut);
}

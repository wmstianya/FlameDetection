/**
 * @file    ads1220Port.h
 * @brief   U27 ADS1220 bit-bang driver; register config aligned with the master.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  A1 dead-sensor policy; C1 IDAC fix (see ads1220Port.c).
 */
#ifndef ADS1220_PORT_H
#define ADS1220_PORT_H

#include <stdint.h>
#include "../config/mcu2Types.h"

#define ADS1220_PORT_OK           0U    /* fresh sample read out          */
#define ADS1220_PORT_NOT_READY    1U    /* DRDY not asserted yet          */
#define ADS1220_PORT_ERROR        2U    /* bad argument / hard error      */

/**
 * @brief  Initialise the bit-bang GPIO and reading-policy state.
 * @return None.
 */
void ads1220PortInit(void);

/**
 * @brief  Reset and configure the ADS1220 registers.
 * @return None.
 */
void ads1220PortConfig(void);

/**
 * @brief  Read a raw 24-bit sample when DRDY is asserted.
 * @param  rawOut Output sign-extended sample (must not be NULL).
 * @return ADS1220_PORT_OK / _NOT_READY / _ERROR.
 */
uint8 ads1220PortTryReadRaw(int32_t *rawOut);

/**
 * @brief  Read furnace temperature with dead-sensor detection (A1).
 * @param  statOut Optional; receives HOST_STAT_OK or HOST_STAT_FAULT.
 * @return Furnace temperature in degrees C (>=900 sentinel on fault).
 */
uint16 ads1220PortReadTempC(uint8 *statOut);

#endif /* ADS1220_PORT_H */

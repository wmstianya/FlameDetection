/**
 * @file    ads1220Port.h
 * @brief   U27 ADS1220 bit-bang driver — config aligned with ADS1220Config()
 */
#ifndef ADS1220_PORT_H
#define ADS1220_PORT_H

#include <stdint.h>
#include "../config/mcu2Types.h"

#define ADS1220_PORT_OK           0U
#define ADS1220_PORT_NOT_READY    1U
#define ADS1220_PORT_ERROR        2U

void ads1220PortInit(void);
void ads1220PortConfig(void);
uint8 ads1220PortTryReadRaw(int32_t *rawOut);
uint16 ads1220PortReadTempC(uint8 *statOut);

#endif /* ADS1220_PORT_H */

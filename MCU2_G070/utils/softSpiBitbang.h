/**
 * @file    softSpiBitbang.h
 * @brief   Bit-bang SPI Mode 1 for ADS1220 on PB3–PB7
 */
#ifndef SOFT_SPI_BITBANG_H
#define SOFT_SPI_BITBANG_H

#include <stdint.h>

void softSpiBitbangInit(void);
void softSpiBitbangSendByte(uint8_t data);
uint8_t softSpiBitbangRecvByte(void);
void softSpiBitbangCsSet(uint8_t enable);

#endif /* SOFT_SPI_BITBANG_H */

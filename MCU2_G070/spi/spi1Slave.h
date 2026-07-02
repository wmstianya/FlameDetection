/**
 * @file    spi1Slave.h
 * @brief   SPI1 slave PA4–PA7 — 5-byte full-duplex exchange
 */
#ifndef SPI1_SLAVE_H
#define SPI1_SLAVE_H

#include "../protocol/hostProtocol.h"
#include "stm32g0xx_hal.h"

void spi1SlaveInit(void);
void spi1SlaveSetResponse(const HostFrame *tx);
uint8 spi1SlaveFrameComplete(void);
void spi1SlavePoll(void);
SPI_HandleTypeDef *spi1SlaveGetHandle(void);

#endif /* SPI1_SLAVE_H */

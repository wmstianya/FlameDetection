/**
 * @file    spiSlave.h
 * @brief   SPI1 slave (PA4-PA7): 5-byte full-duplex furnace-temperature bridge.
 * @details Acts as the SPI slave to the D380 F103 master. Only one SPI slave
 *          role exists, so the driver drops the peripheral index from its API
 *          names; the underlying silicon peripheral is still SPI1. The transfer
 *          is kept continuously armed (first byte preloaded) and re-aligns on
 *          the CS(NSS) rising edge so the master can self-heal a lost link.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.2.0  NSS both-edge EXTI, rising-edge resync, first-byte preload.
 */
#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include "../protocol/hostProtocol.h"
#include "stm32g0xx_hal.h"

/**
 * @brief  Initialise SPI1 as a slave and arm the first (preloaded) transfer.
 * @return None.
 */
void spiSlaveInit(void);

/**
 * @brief  Perform any pending frame re-alignment (abort + re-arm).
 * @details Call once per main-loop iteration. Executes the abort/re-arm
 *          requested by a CS-rising-edge resync or a bus error, in thread
 *          context (outside the ISRs) so no clock is active during the abort.
 * @return None.
 */
void spiSlaveService(void);

/**
 * @brief  Stage the frame transmitted on the next master transaction.
 * @param  tx Frame to copy into the transmit buffer (ignored when NULL).
 * @return None.
 */
void spiSlaveSetResponse(const HostFrame *tx);

/**
 * @brief  Consume the "frame completed" flag set by the transfer ISR.
 * @return 1 if a frame completed since the last call, 0 otherwise.
 */
uint8 spiSlaveFrameComplete(void);

/**
 * @brief  Access the HAL SPI handle (used by the SPI1 IRQ handler).
 * @return Pointer to the driver's SPI_HandleTypeDef.
 */
SPI_HandleTypeDef *spiSlaveGetHandle(void);

#endif /* SPI_SLAVE_H */

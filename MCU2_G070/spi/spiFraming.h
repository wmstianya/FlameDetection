/**
 * @file    spiFraming.h
 * @brief   Pure SPI-slave frame-alignment state machine (NSS-driven self-heal).
 * @details The D380 master can only self-heal a de-synchronised SPI link if the
 *          G070 slave RE-ALIGNS its byte framing on every CS(NSS) rising edge.
 *          If a transaction does not complete (master raises CS mid-frame, a bit
 *          slips, or a bus glitch occurs), the HAL transfer stalls in
 *          BUSY_TX_RX and every subsequent frame is shifted, so the master never
 *          sees a valid 0x68 frame and never recovers.
 *
 *          This module holds the decision logic only (no HAL), so it is
 *          host-unit-testable. The spiSlave driver maps hardware events onto it
 *          and performs the matching HAL actions:
 *            - CS assert (NSS falling)  -> a transaction is in progress.
 *            - transfer complete        -> a full frame arrived; frame ready.
 *            - CS deassert (NSS rising) -> if the transaction had NOT completed,
 *              request a resync (abort + re-arm + first-byte preload).
 *            - bus error                -> request a resync.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.0.0
 */
#ifndef SPI_FRAMING_H
#define SPI_FRAMING_H

#include "../config/mcu2Types.h"

/**
 * @brief Frame-alignment state (encapsulated; no loose globals).
 */
typedef struct
{
    uint8 transactionActive; /**< 1 between CS assert and completion/CS deassert. */
    uint8 resyncPending;     /**< 1 when an abort + re-arm is required.           */
    uint8 frameReady;        /**< 1 when a full 5-byte frame completed.           */
} SpiFraming;

/**
 * @brief  Reset the framing state (caller arms the hardware separately).
 * @param  framing State to initialise (must not be NULL).
 * @return None.
 */
void spiFramingInit(SpiFraming *framing);

/**
 * @brief  CS asserted (NSS falling): a master transaction has begun.
 * @param  framing Framing state (must not be NULL).
 * @return None.
 */
void spiFramingOnCsAssert(SpiFraming *framing);

/**
 * @brief  Transfer completed: a full 5-byte frame was exchanged.
 * @param  framing Framing state (must not be NULL).
 * @return None.
 */
void spiFramingOnComplete(SpiFraming *framing);

/**
 * @brief  CS deasserted (NSS rising): the transaction window has closed.
 * @details If the transfer had not completed, the byte framing is now
 *          ambiguous, so a resync (abort + re-arm) is requested. This is the
 *          hook the master relies on to self-heal.
 * @param  framing Framing state (must not be NULL).
 * @return 1 if a resync is now pending, 0 otherwise.
 */
uint8 spiFramingOnCsDeassert(SpiFraming *framing);

/**
 * @brief  Bus error: request a resync (abort + re-arm).
 * @param  framing Framing state (must not be NULL).
 * @return None.
 */
void spiFramingOnError(SpiFraming *framing);

/**
 * @brief  Read and clear the "resync pending" flag (service context).
 * @param  framing Framing state (must not be NULL).
 * @return 1 if a resync was pending, 0 otherwise.
 */
uint8 spiFramingConsumeResync(SpiFraming *framing);

/**
 * @brief  Read and clear the "frame ready" flag (main-loop context).
 * @param  framing Framing state (must not be NULL).
 * @return 1 if a frame completed since the last call, 0 otherwise.
 */
uint8 spiFramingConsumeFrameReady(SpiFraming *framing);

/**
 * @brief  Whether the response buffer may be updated without tearing a frame.
 * @param  framing Framing state (must not be NULL).
 * @return 1 when no transaction is active (safe to update), 0 otherwise.
 */
uint8 spiFramingAllowResponseUpdate(const SpiFraming *framing);

#endif /* SPI_FRAMING_H */

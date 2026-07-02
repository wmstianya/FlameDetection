/**
 * @file    spiFraming.c
 * @brief   Pure SPI-slave frame-alignment state machine (NSS-driven self-heal).
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.0.0
 */
#include "spiFraming.h"

void spiFramingInit(SpiFraming *framing)
{
    if (framing == NULL)
        return;
    framing->transactionActive = 0U;
    framing->resyncPending = 0U;
    framing->frameReady = 0U;
}

void spiFramingOnCsAssert(SpiFraming *framing)
{
    if (framing == NULL)
        return;
    framing->transactionActive = 1U;
}

void spiFramingOnComplete(SpiFraming *framing)
{
    if (framing == NULL)
        return;
    framing->transactionActive = 0U;
    framing->frameReady = 1U;
}

uint8 spiFramingOnCsDeassert(SpiFraming *framing)
{
    if (framing == NULL)
        return 0U;
    /* A transaction still marked active at CS deassert means the transfer did
     * not complete (short/aborted frame): the byte index is now ambiguous, so
     * demand a resync before the next window. A completed transfer already
     * cleared the flag, so a healthy frame does not force a resync here. */
    if (framing->transactionActive != 0U)
        framing->resyncPending = 1U;
    framing->transactionActive = 0U;
    return framing->resyncPending;
}

void spiFramingOnError(SpiFraming *framing)
{
    if (framing == NULL)
        return;
    framing->transactionActive = 0U;
    framing->resyncPending = 1U;
}

uint8 spiFramingConsumeResync(SpiFraming *framing)
{
    uint8 pending;
    if (framing == NULL)
        return 0U;
    pending = framing->resyncPending;
    framing->resyncPending = 0U;
    return pending;
}

uint8 spiFramingConsumeFrameReady(SpiFraming *framing)
{
    uint8 ready;
    if (framing == NULL)
        return 0U;
    ready = framing->frameReady;
    framing->frameReady = 0U;
    return ready;
}

uint8 spiFramingAllowResponseUpdate(const SpiFraming *framing)
{
    if (framing == NULL)
        return 0U;
    return (framing->transactionActive == 0U) ? 1U : 0U;
}

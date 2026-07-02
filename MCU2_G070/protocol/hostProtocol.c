/**
 * @file    hostProtocol.c
 * @brief   5-byte SPI protocol — aligned with UART/HARDWARE/led/bsp_led.c
 */
#include "hostProtocol.h"

uint8 hostFrameChecksum(const uint8 *frame)
{
    return (uint8)(frame[0] + frame[1] + frame[2] + frame[3]);
}

uint8 hostFrameValid(const uint8 *frame)
{
    if (frame[0] != HOST_FRAME_HEAD)
        return 0U;
    if (hostFrameChecksum(frame) != frame[4])
        return 0U;
    return 1U;
}

uint8 hostParseCommand(const HostFrame *rx, HostCommand *cmd)
{
    const uint8 *f = rx->bytes;
    if (!hostFrameValid(f))
        return 0U;
    if (cmd != NULL)
    {
        cmd->protectLimitC = (uint16)(((uint16)f[1] << 8) | f[2]);
        cmd->yuReEnabled = f[3];
    }
    return 1U;
}

void hostBuildResponse(HostFrame *tx, uint16 tempC, uint8 stat)
{
    uint8 *f = tx->bytes;
    f[0] = HOST_FRAME_HEAD;
    f[1] = (uint8)(tempC >> 8);
    f[2] = (uint8)(tempC & 0xFFU);
    f[3] = stat;
    f[4] = hostFrameChecksum(f);
}

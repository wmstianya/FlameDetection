/**
 * @file    hostProtocol.h
 * @brief   Host/slave 5-byte SPI frame — aligned with bsp_led.c SpiReadData
 */
#ifndef HOST_PROTOCOL_H
#define HOST_PROTOCOL_H

#include "mcu2Types.h"

#define HOST_FRAME_HEAD       0x68U
#define HOST_FRAME_LEN        5U

#define HOST_STAT_OK          0x01U
#define HOST_STAT_FAULT       0x00U
#define HOST_STAT_WARMUP      0x02U

#define HOST_TEMP_DISCONNECT  900U
#define HOST_TEMP_CLAMP_MAX   999U

typedef struct
{
    uint16 protectLimitC;
    uint8  yuReEnabled;
} HostCommand;

typedef struct
{
    uint8 bytes[HOST_FRAME_LEN];
} HostFrame;

uint8 hostFrameChecksum(const uint8 *frame);
uint8 hostParseCommand(const HostFrame *rx, HostCommand *cmd);
void hostBuildResponse(HostFrame *tx, uint16 tempC, uint8 stat);
uint8 hostFrameValid(const uint8 *frame);

#endif /* HOST_PROTOCOL_H */

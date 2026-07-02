/**
 * @file    hostProtocol.h
 * @brief   Host/slave 5-byte SPI frame, aligned with the D380 master bsp_led.c.
 * @details Frame layout (both directions): [HEAD][B1][B2][B3][CSUM] where CSUM
 *          is the 8-bit sum of bytes 0..3. Master->slave carries the protection
 *          limit + pre-heat flag; slave->master carries furnace temperature +
 *          status.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0  Added full API documentation.
 */
#ifndef HOST_PROTOCOL_H
#define HOST_PROTOCOL_H

#include "mcu2Types.h"

#define HOST_FRAME_HEAD       0x68U
#define HOST_FRAME_LEN        5U

#define HOST_STAT_OK          0x01U   /* ADS1220 reading valid                 */
#define HOST_STAT_FAULT       0x00U   /* read failed; TEMP carries a sentinel  */
#define HOST_STAT_WARMUP      0x02U   /* reserved: warming up                  */

#define HOST_TEMP_DISCONNECT  900U    /* >=900 => probe disconnected (Error6)  */
#define HOST_TEMP_CLAMP_MAX   999U    /* master clamps readings above 390 C    */

/** @brief Decoded master->slave command (protection limit + pre-heat flag). */
typedef struct
{
    uint16 protectLimitC;   /**< Furnace protection limit, degrees C. */
    uint8  yuReEnabled;     /**< Pre-heat protection enable (0/1).    */
} HostCommand;

/** @brief Raw 5-byte wire frame. */
typedef struct
{
    uint8 bytes[HOST_FRAME_LEN];
} HostFrame;

/**
 * @brief  Compute the additive checksum of frame bytes 0..3.
 * @param  frame Pointer to at least HOST_FRAME_LEN bytes (must not be NULL).
 * @return 8-bit sum (natural wrap) of bytes 0..3.
 */
uint8 hostFrameChecksum(const uint8 *frame);

/**
 * @brief  Validate frame head and checksum.
 * @param  frame Pointer to at least HOST_FRAME_LEN bytes (must not be NULL).
 * @return 1 when head==0x68 and checksum matches, 0 otherwise.
 */
uint8 hostFrameValid(const uint8 *frame);

/**
 * @brief  Parse a received frame into a HostCommand.
 * @param  rx  Received frame (must not be NULL).
 * @param  cmd Output command; left untouched when NULL or on invalid frame.
 * @return 1 when the frame was valid and parsed, 0 otherwise.
 */
uint8 hostParseCommand(const HostFrame *rx, HostCommand *cmd);

/**
 * @brief  Build a slave->master response frame (temperature + status).
 * @param  tx    Output frame (must not be NULL).
 * @param  tempC Furnace temperature in degrees C.
 * @param  stat  Status byte (HOST_STAT_OK / HOST_STAT_FAULT / HOST_STAT_WARMUP).
 * @return None.
 */
void hostBuildResponse(HostFrame *tx, uint16 tempC, uint8 stat);

#endif /* HOST_PROTOCOL_H */

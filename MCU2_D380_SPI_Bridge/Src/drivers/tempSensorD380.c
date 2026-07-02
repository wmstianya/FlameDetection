/**
  ******************************************************************************
  * @file           : tempSensorD380.c
  * @brief          : D380 SPI temperature sensor driver implementation
  * @author         : Senior Embedded Engineer
  * @date           : 2026-07-02
  * @version        : V1.0.0
  ******************************************************************************
  * @attention
  * D380 SPI protocol (DS18B20-compatible framing over SPI):
  *   1. Assert CS
  *   2. Send D380_REG_SKIP_ROM (0xCC) — address all devices on bus
  *   3. Send D380_REG_CONVERT  (0x44) — start 12-bit conversion
  *   4. Release CS
  *   5. Wait D380_CONVERSION_TIMEOUT_MS
  *   6. Assert CS
  *   7. Send D380_REG_SKIP_ROM (0xCC)
  *   8. Send D380_REG_READ_SCRATCHPAD (0xBE)
  *   9. Read 9 bytes: byte[0..1] = raw temperature, byte[8] = CRC8
  *  10. Release CS
  *
  * Temperature decoding (DS18B20 / D380 12-bit):
  *   raw word = (byte[1] << 8) | byte[0]
  *   °C       = raw / 16.0  (sign-extended 16-bit two's complement)
  *
  * Revision history:
  *   V1.0.0  2026-07-02  Initial release
  ******************************************************************************
  */

#include "tempSensorD380.h"
#include "spiMaster.h"
#include "../utils/crc8.h"
#include "main.h"

/* Private constants ---------------------------------------------------------*/

/** Number of bytes in the D380 scratchpad */
#define SCRATCHPAD_SIZE     9u

/** Index of the CRC byte within the scratchpad */
#define SCRATCHPAD_CRC_IDX  8u

/** Raw LSB index in scratchpad */
#define SCRATCHPAD_TEMP_LSB 0u

/** Raw MSB index in scratchpad */
#define SCRATCHPAD_TEMP_MSB 1u

/** Fixed-point scale: 12-bit resolution → 0.0625 °C per LSB → multiply by
 *  10 for °C×10 representation: factor = 10 / 16 = 0.625 → use integer math:
 *  (raw * 10) / 16 = (raw * 5) / 8  */
#define TEMP_DECODE_NUMERATOR   5
#define TEMP_DECODE_DENOMINATOR 8

/* Private helpers -----------------------------------------------------------*/

/**
  * @brief  Send SKIP_ROM + command byte to D380 over SPI (CS managed here).
  * @param  cmd  Command byte to follow SKIP_ROM.
  * @retval D380Status
  */
static D380Status sendCommand(uint8_t cmd)
{
    SpiMasterStatus spiStatus;

    spiMasterCsAssert();

    spiStatus = spiMasterWriteByte(D380_REG_SKIP_ROM);
    if (spiStatus != SPI_MASTER_OK) {
        spiMasterCsRelease();
        return D380_SPI_ERROR;
    }

    spiStatus = spiMasterWriteByte(cmd);
    spiMasterCsRelease();

    return (spiStatus == SPI_MASTER_OK) ? D380_OK : D380_SPI_ERROR;
}

/**
  * @brief  Send SKIP_ROM then the scratchpad-read command; leave CS asserted.
  * @note   Caller must release CS after the data-read phase.
  * @retval D380Status — D380_SPI_ERROR if either byte fails to transmit.
  */
static D380Status sendReadScratchpadCmd(void)
{
    SpiMasterStatus spiStatus;

    spiStatus = spiMasterWriteByte(D380_REG_SKIP_ROM);
    if (spiStatus != SPI_MASTER_OK) {
        spiMasterCsRelease();
        return D380_SPI_ERROR;
    }

    spiStatus = spiMasterWriteByte(D380_REG_READ_SCRATCHPAD);
    if (spiStatus != SPI_MASTER_OK) {
        spiMasterCsRelease();
        return D380_SPI_ERROR;
    }

    return D380_OK;
}

/**
  * @brief  Read the 9-byte scratchpad from D380 into buffer.
  * @param  buf  Must point to at least SCRATCHPAD_SIZE bytes.
  * @retval D380Status
  */
static D380Status readScratchpad(uint8_t *buf)
{
    D380Status      d380Status;
    SpiMasterStatus spiStatus;

    spiMasterCsAssert();

    d380Status = sendReadScratchpadCmd();
    if (d380Status != D380_OK) { return d380Status; }

    spiStatus = spiMasterTransfer(NULL, buf, SCRATCHPAD_SIZE);
    spiMasterCsRelease();

    return (spiStatus == SPI_MASTER_OK) ? D380_OK : D380_SPI_ERROR;
}

/**
  * @brief  Validate scratchpad CRC8 using Dallas/Maxim polynomial.
  * @param  buf  9-byte scratchpad buffer.
  * @retval D380_OK if CRC matches, D380_CRC_ERROR otherwise.
  */
static D380Status validateCrc(const uint8_t *buf)
{
    uint8_t computed = crc8Compute(buf, SCRATCHPAD_CRC_IDX);
    return (computed == buf[SCRATCHPAD_CRC_IDX]) ? D380_OK : D380_CRC_ERROR;
}

/**
  * @brief  Decode temperature from 9-byte scratchpad.
  * @param  buf      Pointer to 9-byte scratchpad (bytes 0 and 1 used).
  * @param  tempX10  Output temperature in °C × 10.
  * @retval D380_OK or D380_RANGE_ERROR
  */
static D380Status decodeScratchpad(const uint8_t *buf, int16_t *tempX10)
{
    int16_t rawWord = (int16_t)(((uint16_t)buf[SCRATCHPAD_TEMP_MSB] << 8u)
                                | buf[SCRATCHPAD_TEMP_LSB]);
    /* °C × 10 = rawWord × 5 / 8  (equivalent to rawWord / 16 × 10) */
    int16_t result = (int16_t)(((int32_t)rawWord * TEMP_DECODE_NUMERATOR)
                                / TEMP_DECODE_DENOMINATOR);

    if (result < D380_TEMP_MIN_X10 || result > D380_TEMP_MAX_X10) {
        return D380_RANGE_ERROR;
    }

    *tempX10 = result;
    return D380_OK;
}

/* Public API ----------------------------------------------------------------*/

/**
  * @brief  Initialise D380 sensor context with safe defaults.
  */
void d380SensorInit(D380Sensor *sensor)
{
    if (sensor == NULL) { return; }

    sensor->lastTempX10       = TEMP_DEFAULT_X10;
    sensor->lastReadTickMs    = 0u;
    sensor->conversionPending = 0u;
    sensor->convStartTickMs   = 0u;
    sensor->lastStatus        = D380_OK;
}

/**
  * @brief  Issue a "start conversion" command to the D380.
  */
D380Status d380StartConversion(D380Sensor *sensor)
{
    D380Status status;

    if (sensor == NULL) { return D380_PARAM_ERROR; }

    status = sendCommand(D380_REG_CONVERT);
    if (status == D380_OK) {
        sensor->conversionPending = 1u;
        sensor->convStartTickMs   = HAL_GetTick();
    }

    sensor->lastStatus = status;
    return status;
}

/**
  * @brief  Check whether the D380 conversion window has elapsed.
  * @param  sensor  Sensor context.
  * @retval D380_BUSY if the window has not yet passed, D380_OK otherwise.
  */
static D380Status checkConversionReady(D380Sensor *sensor)
{
    if (!sensor->conversionPending) { return D380_OK; }

    if ((HAL_GetTick() - sensor->convStartTickMs) < D380_CONVERSION_TIMEOUT_MS) {
        return D380_BUSY;
    }

    sensor->conversionPending = 0u;
    return D380_OK;
}

/**
  * @brief  Read and decode the D380 scratchpad; update sensor context.
  * @param  sensor   Sensor context.
  * @param  tempX10  Output temperature.
  * @retval D380Status
  */
static D380Status fetchAndDecodeTemperature(D380Sensor *sensor, int16_t *tempX10)
{
    uint8_t    scratchpad[SCRATCHPAD_SIZE];
    D380Status status;

    status = readScratchpad(scratchpad);
    if (status != D380_OK) { return status; }

    status = validateCrc(scratchpad);
    if (status != D380_OK) { return status; }

    status = decodeScratchpad(scratchpad, tempX10);
    if (status == D380_OK) {
        sensor->lastTempX10    = *tempX10;
        sensor->lastReadTickMs = HAL_GetTick();
    }

    return status;
}

/**
  * @brief  Read temperature from D380 scratchpad (non-blocking).
  */
D380Status d380ReadTemperature(D380Sensor *sensor, int16_t *tempX10)
{
    D380Status status;

    if (sensor == NULL || tempX10 == NULL) { return D380_PARAM_ERROR; }

    status = checkConversionReady(sensor);
    if (status != D380_OK) { sensor->lastStatus = status; return status; }

    status = fetchAndDecodeTemperature(sensor, tempX10);
    sensor->lastStatus = status;
    return status;
}

/**
  * @brief  Busy-wait for D380 conversion with periodic watchdog refresh.
  * @note   Blocks for D380_CONVERSION_TIMEOUT_MS ms total.
  */
static void waitForConversionWithWatchdog(void)
{
    extern IWDG_HandleTypeDef hiwdg;
    uint32_t startTick = HAL_GetTick();

    while ((HAL_GetTick() - startTick) < D380_CONVERSION_TIMEOUT_MS) {
        HAL_Delay(100u);
        HAL_IWDG_Refresh(&hiwdg);
    }
}

/**
  * @brief  Blocking convenience: start conversion, wait, then read.
  * @note   Blocks up to D380_CONVERSION_TIMEOUT_MS ms. Kicks watchdog
  *         internally to prevent reset during the long conversion wait.
  */
D380Status d380ReadTemperatureBlocking(D380Sensor *sensor, int16_t *tempX10)
{
    D380Status status;

    if (sensor == NULL || tempX10 == NULL) { return D380_PARAM_ERROR; }

    status = d380StartConversion(sensor);
    if (status != D380_OK) { return status; }

    waitForConversionWithWatchdog();

    sensor->conversionPending = 0u;
    status = d380ReadTemperature(sensor, tempX10);
    return status;
}

/************************ END OF FILE *****************************/

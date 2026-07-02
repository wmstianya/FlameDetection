/**
 * @file    main.c
 * @brief   U31 MCU2 entry: SPI-slave furnace-temperature bridge to the U13 F103.
 * @author  Cursor Agent
 * @date    2026-07-02
 * @version 1.1.0
 */
#include "../board/boardInit.h"
#include "../config/mcu2Types.h"
#include "../protocol/hostProtocol.h"
#include "../spi/spiSlave.h"
#include "../ads1220/ads1220Port.h"

int main(void)
{
    HostFrame response;
    uint16 furnaceTempC;
    uint8 stat;

    boardInit();
    ads1220PortInit();
    ads1220PortConfig();
    spiSlaveInit();

    for (;;)
    {
        wdiFeedToggle();
        spiSlaveService();
        furnaceTempC = ads1220PortReadTempC(&stat);
        hostBuildResponse(&response, furnaceTempC, stat);
        spiSlaveSetResponse(&response);

        if (spiSlaveFrameComplete() != 0U)
            ledComPulse();

        runLedHeartbeat();
    }
}

/**
 * @file    main.c
 * @brief   U31 MCU2 entry — SPI slave furnace temperature to U13 F103
 */
#include "../board/boardInit.h"
#include "../config/mcu2Types.h"
#include "../protocol/hostProtocol.h"
#include "../spi/spi1Slave.h"
#include "../ads1220/ads1220Port.h"

int main(void)
{
    HostFrame response;
    uint16 furnaceTempC;
    uint8 stat;

    boardInit();
    ads1220PortInit();
    ads1220PortConfig();
    spi1SlaveInit();

    for (;;)
    {
        wdiFeedToggle();
        furnaceTempC = ads1220PortReadTempC(&stat);
        hostBuildResponse(&response, furnaceTempC, stat);
        spi1SlaveSetResponse(&response);
        spi1SlavePoll();

        if (spi1SlaveFrameComplete() != 0U)
            ledComPulse();

        runLedHeartbeat();
    }
}

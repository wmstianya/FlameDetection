@echo off
setlocal

set GCC=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-gcc.exe
set OBJCOPY=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objcopy.exe
set SIZE=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-size.exe

set ROOT=%~dp0
set M2=%ROOT%MCU2_G070

set CFLAGS=-mcpu=cortex-m0plus -mthumb -O1 -g -ffunction-sections -fdata-sections -std=c99 -Wall
set DEFS=-DSTM32G070xx -DUSE_HAL_DRIVER
set INCS=-I%M2%/Inc -I%M2%/config -I%M2%/protocol -I%M2%/spi -I%M2%/ads1220 -I%M2%/board -I%M2%/utils -IDrivers/STM32G0xx_HAL_Driver/Inc -IDrivers/STM32G0xx_HAL_Driver/Inc/Legacy -IDrivers/CMSIS/Device/ST/STM32G0xx/Include -IDrivers/CMSIS/Include
set LDFLAGS=-mcpu=cortex-m0plus -mthumb -Wl,--gc-sections --specs=nosys.specs --specs=nano.specs -TSTM32G070CBT6.ld -lc -lm -lnosys

set STARTUP=Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/gcc/startup_stm32g070xx.s

set HAL_SRC=^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_cortex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_exti.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_spi.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_rcc.c

set MCU2_SRC=^
 %M2%/app/main.c^
 %M2%/protocol/hostProtocol.c^
 %M2%/spi/spiSlave.c^
 %M2%/ads1220/ads1220Port.c^
 %M2%/ads1220/ads1220Reading.c^
 %M2%/board/boardInit.c^
 %M2%/utils/softSpiBitbang.c^
 %M2%/utils/tempCalib.c^
 %M2%/Src/mcu2Main.c^
 %M2%/Src/stm32g0xx_it.c^
 %M2%/Src/stm32g0xx_hal_msp.c^
 Src/system_stm32g0xx.c

if not exist build mkdir build

echo === Compiling MCU2_G070 (STM32G070CBT6) ===
cd /d "%ROOT%"
"%GCC%" %CFLAGS% %DEFS% %INCS% %STARTUP% %HAL_SRC% %MCU2_SRC% %LDFLAGS% -o build/mcu2_g070.elf
if errorlevel 1 (
    echo === BUILD FAILED ===
    exit /b 1
)

"%OBJCOPY%" -O ihex build/mcu2_g070.elf build/mcu2_g070.hex
"%OBJCOPY%" -O binary build/mcu2_g070.elf build/mcu2_g070.bin
"%SIZE%" build/mcu2_g070.elf

echo === BUILD SUCCESS ===

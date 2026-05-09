@echo off
setlocal

set GCC=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-gcc.exe
set OBJCOPY=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objcopy.exe
set SIZE=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-size.exe

set CFLAGS=-mcpu=cortex-m0plus -mthumb -O1 -g -ffunction-sections -fdata-sections -std=c99 -Wall
set DEFS=-DSTM32G070xx -DUSE_HAL_DRIVER
set INCS=-IInc -IDrivers/STM32G0xx_HAL_Driver/Inc -IDrivers/STM32G0xx_HAL_Driver/Inc/Legacy -IDrivers/CMSIS/Device/ST/STM32G0xx/Include -IDrivers/CMSIS/Include
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
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_exti.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_iwdg.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_adc.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_adc_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_tim.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_tim_ex.c^
 Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_rcc.c

set APP_SRC=^
 Src/main.c^
 Src/adc.c^
 Src/tm1650.c^
 Src/flame.c^
 Src/stm32g0xx_it.c^
 Src/stm32g0xx_hal_msp.c^
 Src/system_stm32g0xx.c

if not exist build mkdir build

echo === Compiling FlameDetection (STM32G070CBT6) ===
"%GCC%" %CFLAGS% %DEFS% %INCS% %STARTUP% %HAL_SRC% %APP_SRC% %LDFLAGS% -o build/FlameDetection.elf
if errorlevel 1 (
    echo === BUILD FAILED ===
    exit /b 1
)

"%OBJCOPY%" -O ihex build/FlameDetection.elf build/FlameDetection.hex
"%OBJCOPY%" -O binary build/FlameDetection.elf build/FlameDetection.bin
"%SIZE%" build/FlameDetection.elf

echo === BUILD SUCCESS ===

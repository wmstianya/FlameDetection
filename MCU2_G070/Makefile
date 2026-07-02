# MCU2_G070 (STM32G070CBT6) firmware build for arm-none-eabi-gcc.
# Mirrors build_mcu2_gcc.bat but is host/CI friendly (Linux, relative paths).
#
#   make            # build ELF/HEX/BIN into build/
#   make clean

TARGET   := mcu2_g070
BUILDDIR := build

# Repo root is one level above this Makefile.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
M2   := $(ROOT)/MCU2_G070

TOOLPREFIX ?= arm-none-eabi-
CC      := $(TOOLPREFIX)gcc
OBJCOPY := $(TOOLPREFIX)objcopy
SIZE    := $(TOOLPREFIX)size

CPU   := -mcpu=cortex-m0plus -mthumb
DEFS  := -DSTM32G070xx -DUSE_HAL_DRIVER
OPT   := -O1 -g

INCS := \
  -I$(M2)/Inc -I$(M2)/config -I$(M2)/protocol -I$(M2)/spi -I$(M2)/ads1220 \
  -I$(M2)/board -I$(M2)/utils \
  -I$(ROOT)/Drivers/STM32G0xx_HAL_Driver/Inc \
  -I$(ROOT)/Drivers/STM32G0xx_HAL_Driver/Inc/Legacy \
  -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32G0xx/Include \
  -I$(ROOT)/Drivers/CMSIS/Include

CFLAGS  := $(CPU) $(OPT) -ffunction-sections -fdata-sections -std=c99 -Wall $(DEFS) $(INCS)
LDFLAGS := $(CPU) -Wl,--gc-sections --specs=nosys.specs --specs=nano.specs \
           -T$(ROOT)/STM32G070CBT6.ld -lc -lm -lnosys

STARTUP := $(ROOT)/Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/gcc/startup_stm32g070xx.s

HAL_SRC := \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr_ex.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_cortex.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash_ex.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_exti.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma_ex.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_spi.c \
  $(ROOT)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_rcc.c

MCU2_SRC := \
  $(M2)/app/main.c \
  $(M2)/protocol/hostProtocol.c \
  $(M2)/spi/spiSlave.c \
  $(M2)/spi/spiFraming.c \
  $(M2)/ads1220/ads1220Port.c \
  $(M2)/ads1220/ads1220Reading.c \
  $(M2)/board/boardInit.c \
  $(M2)/utils/softSpiBitbang.c \
  $(M2)/utils/tempCalib.c \
  $(M2)/utils/tempFilter.c \
  $(M2)/Src/mcu2Main.c \
  $(M2)/Src/stm32g0xx_it.c \
  $(M2)/Src/stm32g0xx_hal_msp.c \
  $(ROOT)/Src/system_stm32g0xx.c

SRCS := $(STARTUP) $(HAL_SRC) $(MCU2_SRC)

.PHONY: all clean
all: $(BUILDDIR)/$(TARGET).elf

$(BUILDDIR)/$(TARGET).elf: $(SRCS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@
	$(OBJCOPY) -O ihex $@ $(BUILDDIR)/$(TARGET).hex
	$(OBJCOPY) -O binary $@ $(BUILDDIR)/$(TARGET).bin
	$(SIZE) $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

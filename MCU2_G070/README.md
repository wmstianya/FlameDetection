# MCU2_G070 — U31 副芯片固件

STM32G070CBT6 副芯片工程，与 D380 主芯片 (F103) 通过 SPI Mode0 5 字节帧通信。

## 构建

```bat
build_mcu2_gcc.bat
```

输出：`build/mcu2_g070.elf` / `.hex` / `.bin`

## 烧录

```bat
JLink.exe -CommandFile jlink_mcu2_flash.jlink
```

## 文档

见 [docs/MCU2_G070](../docs/MCU2_G070/README.md)

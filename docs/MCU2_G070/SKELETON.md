<!--
 * @Author: Administrator wmstianya@gmail.com
 * @Date: 2026-07-01 15:29:05
 * @LastEditors: Administrator wmstianya@gmail.com
 * @LastEditTime: 2026-07-01 15:31:14
 * @FilePath: \flame_renewe:\data\d380_latest\380_2.06\UART\docs\MCU2_G070\SKELETON.md
 * @Description: 威特斯项目文件
 * 
 * Copyright (c) 2026 by ${git_name_email}, All Rights Reserved. 
-->
# 工程骨架速查

## 仓库路径

```
docs/MCU2_G070/          ← Markdown 规范（本目录）
MCU2_G070/               ← C 源码骨架
UART/HARDWARE/led/       ← 主芯片 SPI 主机实现
```

## 一键对照表

| 副芯片文件 | 主芯片文件 | 函数/宏 |
|------------|------------|---------|
| `protocol/hostProtocol.c` | `bsp_led.c` | `SPI_FRAME_HEAD`, checksum |
| `protocol/hostProtocol.c` | `bsp_led.c` | `TxData[]` / `ReciveData[]` |
| `ads1220/ads1220Port.c` | `ADS1220.c` | `ADS1220Config()` |
| `config/mcu2Pins.h` | `soft_spi_pins.h` | PE7–10 ↔ PA4–7 |
| `spi/spi1Slave.c` | `SPI_RW_Data_MODE0` | Mode0 从机侧 |

## 最小联调（T0）

1. 副芯片 `hostBuildResponse()` 固定 **300 °C**，STAT=**0x01**。
2. 主芯片烧 `cursor/fix-spi-manual-pwm-7da0`。
3. GDB：`p spiStat.rxOk` 应每秒 +1。

## 文档列表

- [README.md](./README.md) — 总览
- [PROTOCOL.md](./PROTOCOL.md) — 协议
- [PINMAP.md](./PINMAP.md) — 引脚
- [ARCHITECTURE.md](./ARCHITECTURE.md) — 架构
- [BUILD.md](./BUILD.md) — 构建联调

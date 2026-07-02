# U31 副芯片 (STM32G070) 工程说明

本目录定义 **D380 主芯片 (U13 / STM32F103VCT6)** 与 **副芯片 (U31 / STM32G070CBT6)** 之间的通信契约与 G070 工程骨架，与主仓库固件 `HARDWARE/led/bsp_led.c` 中 `SpiReadData()` / `spiExchangeFrame()` **逐字节对齐**。

## 文档索引

| 文档 | 内容 |
|------|------|
| [PROTOCOL.md](./PROTOCOL.md) | 5 字节 SPI 帧格式、时序、校验、故障语义（Error17） |
| [PINMAP.md](./PINMAP.md) | U31 LQFP48 引脚与 NET 网络对照 |
| [ARCHITECTURE.md](./ARCHITECTURE.md) | 目录结构、模块接口、与主芯片源码映射 |
| [BUILD.md](./BUILD.md) | CubeMX / GCC / 烧录 / 联调步骤 |
| [SKELETON.md](./SKELETON.md) | 速查：路径对照 + T0 联调 |

## 硬件角色

```
┌──────────────────── U13 主芯片 (F103) ────────────────────┐
│  PE7 SCLK ────────┬── SPI1_CLK ──► PA5                   │
│  PE8 CS   ────────┼── SPI1_CS  ──► PA4                   │
│  PE9 MOSI ────────┼── SPI1_MOSI ◄─ PA7 (交叉: 主出→从入)  │
│  PE10 MISO ◄──────┴── SPI1_MISO ── PA6                   │
│  位 bang MODE0，~100 µs 半周期，1 Hz 轮询 (SPI_ReadRX_Flag) │
└───────────────────────────────────────────────────────────┘
                              │
┌──────────────────── U31 副芯片 (G070) ────────────────────┐
│  SPI1 从机 PA4–PA7                                          │
│  ADS1220 (U27) 软件 SPI：PB3 CS, PB4 SCK, PB5 DRDY,        │
│                        PB6 MISO, PB7 MOSI                   │
│  PB0 → WDI1 (TPS3823)   PD3 → RUN LED   PB1 → COM LED      │
└───────────────────────────────────────────────────────────┘
```

## 主芯片侧参考（只读，勿改引脚）

| 主芯片文件 | 符号 | 说明 |
|------------|------|------|
| `HARDWARE/SOFT_SPI/soft_spi_pins.h` | `SOFT_SPI_TEMP_*` | PE7–PE10 |
| `HARDWARE/led/bsp_led.c` | `SpiReadData()` | 1 Hz 发起 SPI 事务 |
| `HARDWARE/led/bsp_led.c` | `SPI_FRAME_HEAD` `0x68` | 帧头 |
| `HARDWARE/led/bsp_led.h` | `spiStat` | GDB 诊断：`rxOk` / `rxHeadBad` |

## 工程骨架位置

可编译骨架源码在仓库根目录 **`MCU2_G070/`**（与 `UART/` 主工程并列，独立工具链）。

## 验收标准（与主芯片联调）

1. 主芯片 GDB：`p spiStat.rxOk` 每秒递增；`rxHeadBad` 不持续增长。
2. `sys_flag.Protect_WenDu` 为合理炉温（非 0）；`Error17` 清除。
3. 屏上 `LuNei_WenDu` / `Inside_WenDu` 有显示。
4. 副芯片 PB1 在 SPI 通信时闪烁；PB0 周期性翻转喂外部 WDT。

## 版本对齐

| 项 | 值 |
|----|-----|
| 主固件分支 | `cursor/fix-spi-manual-pwm-7da0`（含 SPI 时序 + 手动模式 Error17 修复） |
| 协议版本 | V1（5 字节累加和，头 `0x68`） |
| 副芯片 MCU | STM32G070CBT6 / LQFP48 |

# U31 (STM32G070CBT6) 引脚表

器件：**U31**，备注 STM32G070CBT6，封装 LQFP48。  
与主芯片 **U13 (STM32F103VCT6)** 的 SPI 互联网名：`SPI1_CS/CLK/MISO/MOSI`。

## 主副 SPI 互联（必接）

| NET | U13 主 (F103) 固件 | U31 脚 | U31 端口 | 方向（相对 U31） |
|-----|-------------------|--------|----------|------------------|
| SPI1_CLK | PE7 `SOFT_SPI_TEMP_SCLK` | 16 | PA5 | 输入（从机 SCK） |
| SPI1_CS | PE8 `SOFT_SPI_TEMP_CS` | 15 | PA4 | 输入（从机 NSS） |
| SPI1_MOSI | PE9 `SOFT_SPI_TEMP_MOSI` | 18 | PA7 | 输入（从机 MOSI） |
| SPI1_MISO | PE10 `SOFT_SPI_TEMP_MISO` | 17 | PA6 | 输出（从机 MISO） |

> 命名以 **主机 MOSI/MISO** 为准；U31 侧 PA7=SPI1_MOSI 接主机 PE9，PA6=SPI1_MISO 接主机 PE10。

## 功能分组

### 电源 / 时钟 / 复位 / 调试

| U31脚 | 端口 | NET | 说明 |
|------|------|-----|------|
| 4–6 | VBAT/VREF/VDD | VCC3.3 | 3.3V |
| 7 | VSS | GND | 地 |
| 8–9 | PF0/PF1 | NetC89/90 | 8 MHz HSE (Y2) |
| 10 | NRST | MCU2_RST | 复位，R144 上拉 |
| 35–36 | PA13/PA14 | SWDIO1/SWCLK1 | SWD |

### ADS1220 (U27) — 软件 SPI（禁止硬件 SPI2）

| 信号 | U31脚 | 端口 | 对端 |
|------|------|------|------|
| ADS1220_CS2 | 42 | PB3 | U27 pin2 |
| ADS1220_CLK2 | 43 | PB4 | U27 pin1 |
| ADS1220_DRDY2 | 44 | PB5 | U27 pin14 |
| ADS1220_MISO2 | 45 | PB6 | U27 pin15 |
| ADS1220_MOSI2 | 46 | PB7 | U27 pin16 |

**PB4 不是 STM32G070 SPI2_SCK**，必须位 bang。配置寄存器对齐主芯片 `ADS1220Config()`。

### 看门狗 / 指示灯 / 联锁

| 功能 | U31脚 | 端口 | NET |
|------|------|------|-----|
| 外部 WDT 喂狗 | 19 | PB0 | WDI1 → U32 TPS3823 |
| 通信灯 | 20 | PB1 | com |
| 运行灯 | 41 | PD3 | LED1 |
| 复位联锁 | 13 | PA2 | MCU2_R_RST |

### 模拟前端（U27 保护网络）

| U31脚 | 端口 | NET | 说明 |
|------|------|-----|------|
| 11 | PA0 | GPIO_PA0 | 经 R149 → U27 pin11 |
| 12 | PA1 | GPIO_PA1 | 经 R150 → U27 pin10 |

骨架阶段可只读 ADS1220 数字结果；模拟路径校准时再对接。

## 骨架头文件映射

见 `MCU2_G070/config/mcu2Pins.h` — 与上表一一对应，编译期 `static_assert` 锁脚。

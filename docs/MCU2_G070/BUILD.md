# G070 副芯片构建与联调

## 1. 工具链

| 工具 | 版本建议 |
|------|----------|
| STM32CubeIDE 或 CubeMX | G070 HAL 最新稳定版 |
| GCC | `arm-none-eabi-gcc` 10+ |
| 调试 | ST-Link V2 / J-Link + OpenOCD |
| 逻辑分析仪 | 可选，验证 Mode0 时序 |

## 2. CubeMX 配置清单

### 2.1 RCC

- HSE：8 MHz 晶振（PF0/PF1）
- SYSCLK：64 MHz（PLL from HSE，按 G070 手册配置）
- 供 SPI 从机：**APB 时钟使能 SPI1**

### 2.2 SPI1（从机）

| 参数 | 值 |
|------|-----|
| Mode | **Slave** |
| NSS | **Hard Input** 或 Soft + PA4 GPIO 中断 |
| CPOL | 0 |
| CPHA | 0（1 Edge） |
| Data Size | 8 bit |
| First Bit | MSB |
| Pins | PA4 NSS, PA5 SCK, PA6 MISO, PA7 MOSI |

### 2.3 GPIO 输出

| 引脚 | 模式 | 初始 |
|------|------|------|
| PB0 WDI | PP | 低 |
| PB1 COM | PP | 低 |
| PD3 RUN | PP | 低 |

### 2.4 GPIO 输入

| 引脚 | 模式 |
|------|------|
| PB5 DRDY | 上拉输入 |

### 2.5 GPIO 软件 SPI（ADS1220）

| 引脚 | 模式 |
|------|------|
| PB3 CS | PP 推挽 |
| PB4 SCK | PP |
| PB7 MOSI | PP |
| PB6 MISO | 上拉输入 |

**不要** 在 Cube 里使能 SPI2 到 PB4。

## 3. 集成骨架源码

1. 将 `MCU2_G070/config`、`protocol`、`ads1220`、`board`、`app` 加入 Cube 工程。
2. 在 `spi1Slave.c` 中调用 `hostProtocol` 组包。
3. 从主仓库复制/精简 `ADS1220.c` 中的寄存器常量到 `ads1220Port.c`（仅 PB 引脚版）。

## 4. 编译（GCC 占位 Makefile）

```bash
cd MCU2_G070
# 补全 HAL_SRC、CMSIS 路径后：
make
```

输出：`build/mcu2_g070.elf` / `.bin`

## 5. 烧录

```bash
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
  -c "program build/mcu2_g070.bin 0x08000000 verify reset exit"
```

SWD：PA13/PA14，3.3V，与 U31 NET 一致。

## 6. 与主芯片联调顺序

1. **仅副芯片**：SWD 单调，RUN LED 1 Hz 闪，WDI 翻转。
2. **主+从，T0 固定温度**：`hostBuildResponse()` 填 300 °C，主芯片 `spiStat.rxOk` 增加。
3. **ADS1220 实读**：炉温非 0，Error17 清除。
4. **全系统**：4013/10.1 屏 `Inside_WenDu` 显示；手动模式不再因 Error17 误开风机（主芯片 `8ea8b2d+`）。

## 7. 主芯片侧（联调前确认）

```bat
cd UART\USER
gcc_build.bat
```

分支：`cursor/fix-spi-manual-pwm-7da0`（含 SPI 逐字节 IRQ 屏蔽 + 手动 Error17 修复）。

GDB：

```gdb
p spiStat
p sys_flag.Protect_WenDu
p sys_flag.Error_Code
```

## 8. 常见问题

| 现象 | 检查 |
|------|------|
| `rxHeadBad` 高 | MISO 线序、Mode0、从机未驱动 MISO |
| `rxOk` 增但 TEMP=0 | 从机未填 response 缓冲 |
| Error17 仍在 | CS 极性、5 字节未在同一 CS 窗完成 |
| 主芯片 OK 但炉温乱跳 | ADS1220 配置/探头接线 PB3–7 |

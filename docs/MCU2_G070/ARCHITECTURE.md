# G070 工程架构与目录骨架

## 1. 设计目标

| 目标 | 实现 |
|------|------|
| 协议与主芯片一致 | `hostProtocol` 严格按 `bsp_led.c` 5 字节帧 |
| 引脚与 NET 一致 | `mcu2Pins.h` 锁 PA4–7 / PB3–7 |
| ADS1220 与主工程同配置 | `ads1220Port` 移植 `ADS1220Config()` 寄存器值 |
| 可独立编译烧录 | 与 `UART/` 主工程分离，GCC + OpenOCD |

## 2. 目录树

```
MCU2_G070/
├── README.md                 ← 本目录快速说明
├── Makefile                  ← arm-none-eabi-gcc 入口（需补 HAL/启动文件）
├── STM32G070CBTx_FLASH.ld    ← 链接脚本占位
├── config/
│   └── mcu2Pins.h            ← 引脚宏 + 编译期锁脚
├── protocol/
│   ├── hostProtocol.h        ← 5 字节帧 API
│   └── hostProtocol.c        ← 组包/解析（与主芯片一致）
├── spi/
│   ├── spi1Slave.h           ← SPI1 从机驱动接口
│   └── spi1Slave.c           ← 占位：Cube HAL 实现放此
├── ads1220/
│   ├── ads1220Port.h         ← 位 bang ADS1220 接口
│   └── ads1220Port.c         ← 配置 + 读数 + 温度换算
├── board/
│   ├── boardInit.h
│   └── boardInit.c           ← 时钟/GPIO/WDI/LED
├── app/
│   └── main.c                ← 主循环
└── docs/                     ← 符号链接 → ../docs/MCU2_G070/
```

## 3. 模块依赖

```
main.c
  ├── boardInit
  ├── spi1Slave ──► hostProtocol
  ├── ads1220Port
  └── mcu2Pins
```

## 4. 模块接口（与主芯片映射）

### 4.1 `hostProtocol` ↔ `bsp_led.c`

| 从机 API | 主机符号 | 说明 |
|----------|----------|------|
| `hostFrameChecksum()` | `sumCrc = rx[0]+..+[3]` | 累加和 |
| `hostBuildResponse()` | `ReciveData[]` 语义 | 填 TEMP + STAT |
| `hostParseCommand()` | `TxData[]` 语义 | 解析 PROT / YURE |
| `HOST_FRAME_HEAD 0x68` | `SPI_FRAME_HEAD` | 帧头 |

### 4.2 `ads1220Port` ↔ `HARDWARE/ADS1220/ADS1220.c`

| 从机 API | 主机符号 | 说明 |
|----------|----------|------|
| `ads1220PortInit()` | `ADS1220Init()` | GPIO PB3–7 |
| `ads1220PortConfig()` | `ADS1220Config()` | 同寄存器写入 |
| `ads1220PortReadRaw()` | `ADS1220TryReadData()` | 24 bit 有符号 |
| `ads1220PortReadTempC()` | （副芯片独有） | raw → `Protect_WenDu` uint16 °C |

**`ADS1220Config()` 寄存器（主芯片现值）：**

```c
// Reg0: MUX AIN1/AIN0, GAIN x16
Temp = ADS1220_MUX_1_0 | ADS1220_GAIN_16;
// Reg1: 连续转换
Temp = ADS1220_CC;
// Reg2: 外部基准, 50/60Hz 抑制, IDAC 500µA
Temp = ADS1220_IDAC_500 | 0x50;
// Reg3: IDAC1→AIN2, IDAC2→AIN3
Temp = ADS1220_IDAC1_AIN2 | ADS1220_IDAC2_AIN3;
```

### 4.3 `spi1Slave` — G070 独有

| API | 行为 |
|-----|------|
| `spi1SlaveInit()` | SPI1 Slave Mode0, PA4–7 |
| `spi1SlaveOnExchange()` | NSS 下降沿起，5 字节 ISR 交换 |
| `spi1SlaveSetResponse()` | 写入下一帧 TX 缓冲 |

主机侧 **无** 对应模块（主机为 PE7–10 位 bang 主模式）。

## 5. 主循环伪代码

```c
int main(void)
{
    boardInit();
    ads1220PortInit();
    ads1220PortConfig();
    spi1SlaveInit();

    for (;;)
    {
        wdiFeedToggle();                    /* PB0 ~500ms */
        furnaceTempC = ads1220PortReadTempC();
        hostBuildResponse(&txFrame, furnaceTempC, HOST_STAT_OK);
        spi1SlaveSetResponse(&txFrame);

        if (spi1SlaveFrameComplete())
            ledComPulse();                  /* PB1 */
        runLedHeartbeat();                  /* PD3 */
    }
}
```

## 6. 状态字节 STAT 定义（V1）

| 值 | 宏 | 含义 |
|----|-----|------|
| `0x01` | `HOST_STAT_OK` | ADS1220 读数有效 |
| `0x00` | `HOST_STAT_FAULT` | 读数失败，TEMP 建议 999 |
| `0x02` | `HOST_STAT_WARMUP` | 预留：预热中 |

主机 V1 **不解析 STAT**，仅用于逻辑分析仪/副芯片调试。

## 7. 待实现清单（CubeMX 生成后填入）

- [ ] `system_stm32g0xx.c` / `startup_stm32g070xx.s`
- [ ] `stm32g0xx_hal_msp.c` — SPI1 GPIO AF
- [ ] `spi1Slave.c` — HAL_SPI 从机 + NSS  EXTI
- [ ] `ads1220Port.c` — 从主工程精简位 bang 时序
- [ ] `STM32G070CBTx_FLASH.ld`
- [ ] OpenOCD / ST-Link 烧录脚本

## 8. 测试阶段

| 阶段 | 从机行为 | 主芯片验收 |
|------|----------|------------|
| T0 | 固定 TEMP=300, STAT=1 | `rxOk++`, Error17 消失 |
| T1 | ADS1220 实时温度 | 炉温随加热变化 |
| T2 | WDI + LED | 外部 WDT 不复位 |
| T3 | 断探头 TEMP≥900 | Error6 路径（可选） |

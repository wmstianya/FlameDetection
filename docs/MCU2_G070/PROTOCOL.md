# 主副芯片 SPI 协议规范（V1）

> **权威实现（主机）**：`UART/HARDWARE/led/bsp_led.c` — `SpiReadData()`、`spiExchangeFrame()`、`SPI_RW_Data_MODE0()`  
> **权威实现（从机，骨架）**：`MCU2_G070/protocol/hostProtocol.c`

---

## 1. 物理层

| 参数 | 值 | 主机代码依据 |
|------|-----|--------------|
| 接口 | 4 线 SPI + CS | PE7–PE10 |
| 模式 | **SPI Mode 0** (CPOL=0, CPHA=0) | `SPI_RW_Data_MODE0` |
| 位序 | MSB first | 左移 `sendData&0x80` |
| CS 极性 | **低有效** | `SpiCsStatus(SPI_ENABLE)` → CS 拉低 |
| 半周期延时 | **~100 µs** | `spiTempDelayUs(100U)` |
| CS setup/hold | **~20 µs** | `spiTempDelayUs(20U)` |
| 每帧字节数 | **5** | `SPI_FRAME_LEN` |
| 主机轮询 | **1 Hz** | `sys_flag.SPI_ReadRX_Flag` @ 1 s tick |
| 重试 | 最多 3 次，间隔 2 ms | `SPI_MAX_ATTEMPTS` / `SPI_RETRY_GAP_MS` |

### 1.1 单字节 MODE0 时序（主机）

```
CS ‾‾‾\___________________________/‾‾‾
SCLK ___/‾\__/‾\__/‾\__/‾\__/‾\__/‾\___
MOSI ----< D7 >< D6 >< ... >< D0 >----
MISO ----< R7 >< R6 >< ... >< R0 >----
         ↑ 上升沿采样 MISO
```

主机在每个 bit：**先稳定 MOSI → 延时 → SCLK 高 → 读 MISO → 延时 → SCLK 低**。

### 1.2 从机实现要点（G070）

- **必须在同一 CS 低电平窗口内**完成 5 字节全双工交换；主机按字节 `[0]→[4]` 顺序时钟。
- 从机 SPI1 配置为 **Slave, Mode 0, 8-bit, NSS 软管理或硬件 NSS 输入 PA4**。
- 若使用中断/DMA，响应字节需 **预先准备好** 或在中断里按字节填充 TX 移位寄存器；**禁止**收完 5 字节再回包。

---

## 2. 链路层 — 5 字节帧

### 2.1 帧头与校验

```c
#define HOST_FRAME_HEAD   0x68U
#define HOST_FRAME_LEN    5U

static uint8 hostFrameChecksum(const uint8 *frame)
{
    return (uint8)(frame[0] + frame[1] + frame[2] + frame[3]);
}
```

校验规则：`frame[4] == hostFrameChecksum(frame)`。  
主机拒收条件：`rx[0] != 0x68` 或 checksum 错误 → `spiStat.rxHeadBad` / `rxCrcBad++`。

### 2.2 主机 → 从机（Tx，主 MOSI）

| 字节 | 名称 | 类型 | 说明 |
|------|------|------|------|
| 0 | HEAD | `uint8` | 固定 `0x68` |
| 1 | PROT_H | `uint8` | 炉温保护阈值高字节 |
| 2 | PROT_L | `uint8` | 炉温保护阈值低字节 |
| 3 | YURE | `uint8` | 预热保护使能 `YuRe_Enabled`（0/1） |
| 4 | CSUM | `uint8` | 字节 0–3 累加和（uint8 溢出自然截断） |

主机组包（摘自 `SpiReadData()`）：

```c
TxData[0] = 0x68;
TxData[1] = Sys_Admin.Inside_WenDu_ProtectValue >> 8;
TxData[2] = Sys_Admin.Inside_WenDu_ProtectValue & 0xFF;
TxData[3] = Sys_Admin.YuRe_Enabled;
TxData[4] = TxData[0] + TxData[1] + TxData[2] + TxData[3];
```

**从机必须解析但不必回显**：PROT 与 YURE 供从机本地告警逻辑扩展；当前骨架仅缓存。

### 2.3 从机 → 主机（Rx，从 MISO）

| 字节 | 名称 | 类型 | 说明 |
|------|------|------|------|
| 0 | HEAD | `uint8` | 固定 `0x68` |
| 1 | TEMP_H | `uint8` | 炉温高字节 |
| 2 | TEMP_L | `uint8` | 炉温低字节 |
| 3 | STAT | `uint8` | 状态，**正常通信固定 `0x01`** |
| 4 | CSUM | `uint8` | 字节 0–3 累加和 |

主机解析：

```c
sys_flag.Protect_WenDu = ReciveData[1] * 256 + ReciveData[2];
LCD10D.DLCD.LuNei_WenDu = sys_flag.Protect_WenDu;
```

### 2.4 炉温数值语义（`Protect_WenDu`）

| 范围 / 值 | 含义 | 主机行为 |
|-----------|------|----------|
| 正常 | 整数 **°C**（如 25–390） | 显示炉温 |
| `> 390` | 异常高 | 主机钳位为 **999** |
| `>= Inside_WenDu_ProtectValue` 且 `YuRe_Enabled` | 超温 | 累计后 Error14 / Error6 |
| `>= 900` | 传感器未接/断线哨兵 | Error6 本体温度未接 |
| 通信失败 15 次 | SPI 无有效帧 | **Error17** `OutWenKong_TxBad` |

从机 ADS1220 换算后应输出 **uint16 °C**；断线时建议回 **≥900** 或由从机 STAT 区分（骨架默认读失败回 999 + STAT=0）。

---

## 3. 事务流程

```
主机 (1 Hz)                         从机 (G070)
    |                                    |
    | CS↓ setup 20µs                     | NSS↓ 检测
    | 字节0: Tx[0] ↔ Rx[0] 同步交换      | SPI1 从机移位
    | 字节1..4 同上                      |
    | CS↑ hold 20µs                      | NSS↑ 完成
    | 校验 Rx                            |
    | ok → Protect_WenDu 更新            | 后台循环: ADS1220 采样刷新 temp
```

从机应在 **两次 SPI 事务之间** 更新 `gHostCtx.furnaceTempC`（ADS1220 滤波值），SPI ISR 内只读该变量组包，避免在 SPI 回调里读 ADS1220（耗时过长）。

---

## 4. 与主芯片故障码对应

| 主机现象 | 条件 | 从机应检查 |
|----------|------|------------|
| Error17 | 连续 >15 次无有效帧 | HEAD/CSUM、MISO 接线、Mode0、NSS |
| 炉温 0 | 通信 OK 但 TEMP=0 | ADS1220 初始化 / U27 接线 |
| Error6 | TEMP≥900 | 探头断线哨兵或换算错误 |
| 每秒短鸣 | Error17 锁存 | 先修 SPI，再验手动模式逻辑 |

---

## 5. 联调 GDB 命令（主芯片 J-Link）

```gdb
p spiStat
p spiStat.rxOk
p spiStat.rxHeadBad
p spiStat.lastRx[0]@5
p sys_flag.Protect_WenDu
p sys_flag.Error_Code
```

期望：`rxOk` 递增，`lastRx[0]==0x68`，`Protect_WenDu` 合理。

---

## 6. 协议变更策略

- 帧头 **不得** 改为非 `0x68`（主机写死 `SPI_FRAME_HEAD`）。
- 增字节须 **同时** 修改主机 `SPI_FRAME_LEN` 与从机；V1 禁止单方面扩展。
- 副芯片 STAT 字节：V1 主机 **不解析**，仅作诊断预留。

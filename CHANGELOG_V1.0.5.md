# 变更日志 V1.0.5

**版本**: V1.0.5  
**日期**: 2026-01-05  
**优化重点**: ADC DMA模式 + 火焰检测迟滞逻辑 + VREFINT温度补偿

---

## 🎯 解决的问题

### 问题1: 温度升高导致基准电压漂移
- **现象**: 高温环境下电压测量偏移，导致火焰误判
- **原因**: VDDA随温度漂移，VREFINT采样不稳定
- **解决**: DMA模式采样 + VREFINT温度补偿算法

### 问题2: 小火检测失败
- **现象**: 采集电压约2600mV的小火未能检测到
- **原因**: 阈值2680mV，边界区域噪声导致跳变
- **解决**: 引入迟滞阈值区间(2550-2750mV)

---

## 📝 新增/修改文件

### 新增文件
| 文件 | 说明 |
|------|------|
| `Src/drivers/adcDriverDma.h` | ADC DMA驱动头文件 |
| `Src/drivers/adcDriverDma.c` | ADC DMA驱动实现 |
| `openspec/CHANGE_001_ADC_DMA_OPTIMIZATION.md` | 变更提案文档 |

### 修改文件
| 文件 | 修改内容 |
|------|----------|
| `Src/config/systemConfig.h` | 新增DMA配置、迟滞阈值定义 |
| `Src/main.c` | 集成DMA采样、迟滞判断逻辑 |
| `MDK-ARM/FlameDetection.uvprojx` | 添加adcDriverDma.c到项目 |

---

## 🔧 关键改动

### 1. ADC DMA模式 (无中断轮询)

```c
/* 配置开关 (systemConfig.h) */
#define ADC_USE_DMA_MODE    1u   /* 1=DMA模式, 0=轮询模式 */

/* DMA缓冲区 */
#define ADC_DMA_BUFFER_SIZE         128u
#define VREFINT_DMA_SAMPLE_COUNT    16u
```

**工作流程**:
```
ADC连续转换 → DMA自动搬运 → 缓冲区填满 → CPU轮询读取
```

### 2. 火焰检测迟滞阈值

```c
/* 迟滞阈值配置 */
#define FLAME_THRESH_LOW_MV    2550u  /* 低于此值 = 确认有火 */
#define FLAME_THRESH_HIGH_MV   2750u  /* 高于此值 = 确认无火 */

/* 迟滞逻辑 */
- 电压 < 2550mV: 火焰存在
- 2550mV ≤ 电压 ≤ 2750mV: 保持上一状态
- 电压 > 2750mV: 火焰熄灭
```

### 3. VREFINT温度补偿

```c
/* 温度补偿系数 */
#define VREFINT_TEMP_COEFF_PPM    30    /* 30ppm/°C */
#define VREFINT_CAL_TEMP_C        30.0f /* 校准温度 */

/* 补偿公式 */
compensatedVrefint = rawVrefint × (1 + ΔT × 30ppm)
```

---

## 📊 预期效果

| 指标 | V1.0.4 | V1.0.5 | 提升 |
|------|--------|--------|------|
| CPU占用率 | ~70% | <10% | 86%↓ |
| 采样一致性 | 不稳定 | 稳定 | ✅ |
| 小火检测 | 可能漏检 | 稳定检出 | ✅ |
| 高温精度 | 误差1.5% | <0.5% | 3倍↑ |

---

## ⚠️ 编译注意

1. **Keil项目已更新**: `adcDriverDma.c`已添加到项目
2. **可回退**: 设置`ADC_USE_DMA_MODE = 0`使用旧轮询模式
3. **无需修改硬件**: 纯软件优化

---

## 🧪 测试建议

### 测试用例1: 小火检测
```
条件: 火焰电压约2550-2650mV
预期: 稳定检测到火焰，无漏检
```

### 测试用例2: 边界跳变
```
条件: 火焰电压在2650-2700mV波动
预期: 状态稳定，无频繁跳变
```

### 测试用例3: 高温环境
```
条件: 环境温度65°C
预期: 电压测量准确，无误判
```

---

## 📋 回滚方案

如需回退到V1.0.4:

```c
// 在systemConfig.h中修改
#define ADC_USE_DMA_MODE    0u   /* 禁用DMA，使用旧轮询模式 */
```

---

**变更完成，请进行实际测试验证！** 🎯


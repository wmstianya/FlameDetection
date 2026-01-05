# 火焰检测系统 FlameDetection

## 项目简介

基于STM32G070CBTx的火焰检测嵌入式系统，支持ADC DMA采集、温度补偿和火焰迟滞检测。

## 版本信息

**当前版本**: V1.0.5  
**更新日期**: 2026-01-05

## 主要特性

- ✅ **ADC DMA模式采集** - CPU占用低于10%
- ✅ **火焰迟滞检测** - 避免边界跳变误判
- ✅ **VREFINT温度补偿** - 高温环境精度提升
- ✅ **小火稳定检测** - 2550-2650mV范围可靠检出

## 硬件平台

- MCU: STM32G070CBTx (Cortex-M0+, 64KB Flash, 36KB RAM)
- 编译器: Keil MDK-ARM
- HAL库: STM32G0xx HAL Driver

## 项目结构

```
FlameDetection/
├── Src/
│   ├── config/          # 配置管理
│   ├── drivers/         # 硬件驱动 (包含DMA驱动)
│   ├── utils/           # 滤波算法
│   └── application/     # 火焰检测逻辑
├── Inc/                 # 头文件
├── MDK-ARM/             # Keil项目
├── openspec/            # OpenSpec变更提案
└── Drivers/             # ST HAL库

```

## 编译说明

1. 打开 `MDK-ARM/FlameDetection.uvprojx`
2. 选择目标平台 `FlameDetection`
3. 编译项目 (F7)
4. 烧录到STM32G070CBTx

## 配置说明

在 `Src/config/systemConfig.h` 中配置:

```c
/* DMA模式开关 */
#define ADC_USE_DMA_MODE    1u   // 1=DMA模式, 0=轮询模式

/* 火焰检测阈值 */
#define FLAME_THRESH_LOW_MV    2550u
#define FLAME_THRESH_HIGH_MV   2750u
```

## 版本历史

- **V1.0.5** (2026-01-05): ADC DMA模式、迟滞阈值、Bug修复
- **V1.0.4** (2025-11-15): 代码重构、模块化
- **V1.0.2** (2025-03-31): 阈值调整
- **V1.0.1** (2025-02-19): 初始版本

## 文档

- [变更日志](CHANGELOG_V1.0.5.md)
- [OpenSpec变更提案](openspec/CHANGE_001_ADC_DMA_OPTIMIZATION.md)
- [硬件校准指南](HARDWARE_CALIBRATION.md)

## License

STM32 HAL库遵循BSD 3-Clause License


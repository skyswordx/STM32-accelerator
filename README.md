<div align="center">

**简体中文** | [English](README.en.md)

<img src="assets-of-README/wave-reconstruction.gif" alt="波形重建演示" width="920" />
<!-- ![Waveform Reconstruction Demo](assets-of-README/image-demo.jpg) -->
演示说明：学习建模完成后，根据频响映射进行时域波形重建并输出。

# STM32-accelerator

2025 电赛信号类 G 题：基于 STM32H750 的黑箱电路学习、频域辨识与实时波形重建系统

[![Platform](https://img.shields.io/badge/Platform-STM32H750-blue?style=flat-square&logo=stmicroelectronics)](https://www.st.com/en/microcontrollers-microprocessors/stm32h750vb.html)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-0A7B83?style=flat-square)](https://www.freertos.org/)
[![DSP](https://img.shields.io/badge/DSP-CMSIS--DSP-6C63FF?style=flat-square)](https://arm-software.github.io/CMSIS-DSP/main/)
[![ADC](https://img.shields.io/badge/ADC-Dual%20Sync%2014bit-orange?style=flat-square)](./Core)
[![Signal%20Path](https://img.shields.io/badge/Signal%20Path-AD9954%20%2B%20DAC-green?style=flat-square)](./Module)
[![Toolchain](https://img.shields.io/badge/Toolchain-CubeMX%20%2B%20MDK--ARM-red?style=flat-square)](./MDK-ARM)

---

</div>

## 项目简介

本项目面向 2025 电赛信号类 G 题（电路模型探究装置），实现了“学习-辨识-重建”的完整闭环：

- 学习阶段：使用 `AD9954` 扫频激励未知电路，双 `ADC` 同步采样输入/输出
- 辨识阶段：通过 `FFT` 与复数频响计算，提取 `H(jω)` 并进行滤波器类型判别
- 重建阶段：分析输入信号谐波，叠加幅相映射后在 `DAC` 端实时输出重建波形

核心链路：

`激励输出（AD9954/片内DAC） -> 双ADC同步采样 -> 频域分析与辨识 -> 传函映射 -> 谐波重建 -> DAC/DDS输出`


## 主要特性

- 双 ADC 同步采样：`ADC_DUALMODE_REGSIMULT` + DMA，强调相位一致性
- 高频域分析：`FFT_LENGTH=4096`，支持窗函数与频谱插值
- 双模式运行：`ADC_MODE_SWEEP`（扫频学习）与 `ADC_MODE_RECONSTRUCT`（重建输出）
- 软件 DDS 输出：支持重建波形缓存更新与实时输出
- 工程化验证：包含 `RigolController` 自动化测量脚本与统计资产

## 项目结构

```text
STM32-accelerator/
├── Core/                      # CubeMX 生成的核心启动/中断/RTOS入口
├── Drivers/                   # HAL/CMSIS 与外设驱动
├── Module/                    # 业务模块（ADC / DAC / Frequency / Parser / UART / Timer）
├── MDK-ARM/                   # Keil 工程与工程配置
├── RigolController/           # 上位机自动测量脚本（频率/幅值扫描）
├── Document/                  # 设计与优化文档
├── assets-of-README/          # README 资源
├── wave_construct.md          # 波形重建相关记录
└── 项目总档案-2025电赛G题.md  # 上级项目归档（位于上级目录）
```

## 技术栈

| 类别 | 方案 |
|---|---|
| 主控 | `STM32H750VBT6` |
| 采样链路 | 双 `ADC` 同步采样 + DMA + `TIM3` 触发 |
| 输出链路 | `AD9954`（激励）+ 片内 `DAC`（重建输出） |
| 频域算法 | `CMSIS-DSP` FFT、窗函数、频谱插值 |
| 实时调度 | `FreeRTOS` |
| 开发工具 | `STM32CubeMX` + `Keil MDK-ARM` |
| 验证工具 | Python + Rigol 自动化测量脚本 |

## 性能快照（含实验条件）

### 关键实验配置

| 项目 | 配置 |
|---|---|
| ADC 模式 | `TIM3` 触发 + `ADC_DUALMODE_REGSIMULT` |
| ADC 分辨率 | `14bit` |
| 采样率 | `Fs ≈ 409.84 kHz` |
| FFT 点数 | `4096` |
| 扫频范围 | `100 Hz ~ 50 kHz`，步进 `100 Hz` |
| 频率分辨率 | `Δf ≈ 100.06 Hz` |
| 分析窗口长度 | `T_window ≈ 10 ms` |
| 输出触发 | `TIM4`（DAC），`TIM6`（重建周期触发） |

### 代表性结果（归档数据）

| 指标 | 观测值 | 来源 |
|---|---|---|
| 已知模型输出控制误差 | `3.20% ~ 5.68%` | 电赛报告（PDF） |
| 频率输出误差（100Hz~1MHz） | `0.1% ~ 2.4%` | 电赛报告（PDF） |
| 频率误差均值 | `0.0295%` | `RigolController/STM32H7_DDS_Frequency_Sweep_Test_20250730_215545.xlsx` |
| 频率误差最大绝对值 | `1.1941%` | 同上 |
| 幅度误差均值 | `-4.0379%` | 同上 |
| 幅度误差最大绝对值 | `13.7%` | 同上 |

> 注：幅值误差受输出链路标定、频段、负载与探头配置影响，后续可继续做分频段补偿。

## 快速开始

### 1) 打开固件工程

- Keil 工程：`MDK-ARM/FinallAttackProject.uvprojx`
- 关键模式：扫频学习（`S5`）与信号重建（`S6`）

### 2) 构建与烧录

1. 在 Keil 中选择目标并编译
2. 下载到 `STM32H750` 开发板
3. 打开串口观察日志与状态切换

### 3) 上位机自动化测量（可选）

```bash
cd RigolController
python simple_test.py
```

用于快速评估频率/幅度误差并导出测试结果。

## 文档索引

- 总体归档：`../项目总档案-2025电赛G题.md`
- 频域模块说明：`Module/Frequency/README.md`
- 自动测量总结：`RigolController/PROJECT_SUMMARY.md`
- 波形重建记录：`wave_construct.md`
- ADC 数据流说明：`adc-dma-底层api.md`

## 致谢

- FPGA/MCU SPI 通信参考：[`FPGA_MCU_SPI_COM`](https://gitee.com/themql/FPGA_MCU_SPI_COM)
- STM32H7 高速采样讨论参考：[`21ic 论坛帖子`](https://bbs.21ic.com/icview-3409296-1-1.html)

## 许可证

当前目录未单独附带许可证文件，默认遵循仓库根目录的许可与使用约定。
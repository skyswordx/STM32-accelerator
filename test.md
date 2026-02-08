# STM32-accelerator 项目架构分析文档

> 本文档分析了 STM32-accelerator 项目的结构，特别是 ADC、DAC、Frequency、Parser、UART 这几个核心模块，并总结了程序入口和 RTOS 任务的架构图。

---

## 目录

- [系统概览](#系统概览)
- [程序入口与初始化流程](#程序入口与初始化流程)
- [RTOS 任务架构](#rtos任务架构)
- [模块详细架构](#模块详细架构)
  - [ADC 模块](#1-adc模块-moduleadc)
  - [DAC 模块](#2-dac模块-moduledac)
  - [Frequency 模块](#3-frequency模块-modulefrequency)
  - [Parser 模块](#4-parser模块-moduleparser)
  - [UART 模块](#5-uart模块-moduleuart)
- [数据流与控制流](#数据流与控制流)
- [核心功能流程](#核心功能流程)
- [主要数据结构](#主要数据结构)
- [硬件接口](#硬件接口)
- [总结](#总结)

---

# STM32-accelerator 项目架构总结

## 系统概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        STM32H7 主控芯片                                  │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐      │
│  │   程序入口      │    │   RTOS调度器    │    │   硬件外设      │      │
│  │   main.c        │────│   FreeRTOS      │────│   ADC/DAC/UART  │      │
│  └─────────────────┘    └─────────────────┘    └─────────────────┘      │
└─────────────────────────────────────────────────────────────────────────┘
```

## 程序入口与初始化流程

```
main.c
├── 系统初始化
│   ├── MPU_Config()
│   ├── SCB_EnableDCache()
│   ├── HAL_Init()
│   ├── SystemClock_Config()
│   └── 外设初始化
│       ├── MX_GPIO_Init()
│       ├── MX_DMA_Init()
│       ├── MX_ADC1_Init() / MX_ADC2_Init()
│       ├── MX_DAC1_Init()
│       ├── MX_USART1_UART_Init() / MX_USART6_UART_Init()
│       ├── MX_SPI1_Init()
│       └── MX_TIM3_Init() / MX_TIM4_Init() / MX_TIM6_Init()
│
└── RTOS任务创建与启动
    ├── osKernelInitialize()
    ├── 任务创建
    │   ├── defaultTaskHandle
    │   ├── ADCProcessingTaskHandle
    │   ├── UARTProcessingTaskHandle
    │   └── myParserTaskHandle
    └── osKernelStart()
```

## RTOS 任务架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                            FreeRTOS 任务调度                              │
├──────────────────┬──────────────────┬──────────────────┬─────────────────┤
│  ADCProcessing   │  UARTProcessing  │   ParserTask     │  DefaultTask    │
│      Task        │      Task        │                  │                 │
│                  │                  │                  │                 │
│  ┌─────────────┐ │  ┌─────────────┐ │  ┌─────────────┐ │ ┌─────────────┐ │
│  │ 数据采集    │ │  │ 串口通信    │ │  │ 功能解析    │ │ │ 系统监控    │ │
│  │ 频谱分析    │ │  │ 命令解析    │ │  │ 信号处理    │ │ │             │ │
│  │ 滤波器辨识  │ │  │ 参数配置    │ │  │             │ │ │             │ │
│  │ 信号重建    │ │  │             │ │  │             │ │ │             │ │
│  └─────────────┘ │  └─────────────┘ │  └─────────────┘ │ └─────────────┘ │
└──────────────────┴──────────────────┴──────────────────┴─────────────────┘
```

## 模块详细架构

### 1. ADC 模块 (Module/ADC)

**功能描述：** 负责高精度数据采集和信号分析处理

```
ADC模块
├── my_adc_task.h/c        # 主要ADC任务实现
│   ├── StartADCProcessingTask()    # RTOS任务入口
│   ├── 数据采集功能
│   │   ├── 双ADC同步采样 (ADC1 + ADC2)
│   │   ├── DMA传输 (4096点, 14位分辨率)
│   │   └── HAL_ADC_ConvCpltCallback() # DMA完成回调
│   ├── 工作模式
│   │   ├── ADC_MODE_IDLE          # 空闲模式
│   │   ├── ADC_MODE_SWEEP         # 扫频模式 (S5命令)
│   │   └── ADC_MODE_RECONSTRUCT   # 信号重建模式 (S6命令)
│   └── 触发方式
│       ├── 按键触发 (GPIO PC1)
│       ├── S5命令触发 (扫频重建)
│       └── S6命令触发 (信号重建)
└── backup.c / ref.c       # 备份和参考代码
```

**关键特性：**

- 采样率：409.84kHz (可配置到 2MHz)
- 分辨率：14 位
- 缓冲区大小：4096 个采样点
- 支持实时信号处理和离线分析

### 2. DAC 模块 (Module/DAC)

**功能描述：** 实现高精度信号生成和波形输出

```
DAC模块
├── my_dac_config.h/c      # DAC基础配置
└── my_dds.h/c             # 数字直接合成器 (DDS)
    ├── DDS_Generator_t     # DDS控制结构体
    │   ├── phase_accumulator      # 相位累加器
    │   ├── frequency_control_word # 频率控制字
    │   ├── wave_table            # 波形查找表
    │   └── dma_buffer           # DMA缓冲区
    ├── 波形生成功能
    │   ├── DDS_Init()           # DDS初始化
    │   ├── DDS_SetFrequency()   # 设置输出频率
    │   ├── DDS_SetWaveform()    # 设置波形类型
    │   └── DDS_Start()          # 启动DDS输出
    └── 支持波形类型
        ├── 正弦波 (g_sine_wave_64[])
        ├── 方波 (g_square_wave_64[])
        └── 三角波 (g_triangle_wave_64[])
```

**关键特性：**

- 更新频率：995.062 kHz
- 波形表大小：64 点
- DMA 缓冲区：128 点
- 支持实时波形切换和双缓冲输出

### 3. Frequency 模块 (Module/Frequency)

**功能描述：** 提供高精度频域分析和信号处理算法

```
Frequency模块
├── my_freq_config.h/c     # 频域分析核心
│   ├── FFT配置 (4096点)
│   ├── 窗函数支持
│   │   ├── WINDOW_NONE       # 无窗函数
│   │   ├── WINDOW_HANNING    # 汉宁窗
│   │   └── WINDOW_FLAT_TOP   # 平顶窗
│   ├── 频谱插值
│   │   ├── INTERPOLATION_PARABOLIC     # 抛物线插值
│   │   └── INTERPOLATION_HANNING_SPECIAL # 汉宁窗专用插值
│   └── fundamental_result_t # 基波分析结果结构
│       ├── fundamental_vpp       # 基波峰峰值
│       ├── fundamental_frequency # 基波频率
│       └── fundamental_phase     # 基波相位
├── my_fir_config.h/c      # FIR滤波器配置
└── README.md / *.md       # 文档说明
```

**关键特性：**

- FFT 长度：4096 点 (高分辨率频谱分析)
- 支持多种窗函数以减少频谱泄漏
- 频谱插值算法提高频率测量精度
- 基于 ARM DSP 库优化的高性能计算

### 4. Parser 模块 (Module/Parser)

```
Parser模块
├── my_parser.h/c          # 功能解析器主控
│   ├── 功能模式定义
│   │   ├── FUNCTION_MODE_BASE2    # 基础功能2
│   │   ├── FUNCTION_MODE_BASE3    # 基础功能3
│   │   ├── FUNCTION_MODE_BASE4    # 基础功能4
│   │   ├── FUNCTION_MODE_IMPROVE1 # 改进功能1 (扫频重建)
│   │   └── FUNCTION_MODE_IMPROVE2 # 改进功能2 (信号重建)
│   ├── myParserTask()     # RTOS任务入口
│   └── base2/3/4_function() # 基础功能实现
├── filter_identification.h/c # 滤波器辨识算法
│   ├── 扫频参数 (100Hz - 50kHz, 491个频点)
│   ├── ContinuousTransferFunction # 传递函数结构
│   │   ├── H(s) = (b2*s² + b1*s + b0) / (s² + a1*s + a0)
│   │   └── FilterType (LPF/HPF/BPF/BSF)
│   └── identify_filter()  # 主要辨识函数
└── my_signal_reconstruction.h/c # 信号重建算法
    ├── harmonic_component_t # 谐波分量结构
    ├── analysis_method_t    # 分析方法枚举
    ├── analyze_and_select_best_method() # 最佳方法选择
    └── reconstruct_output_waveform()   # 波形重建
```

### 5. UART 模块 (Module/UART)

```
UART模块
└── my_uart_task.h/c       # 串口通信任务
    ├── StartUARTProcessingTask() # RTOS任务入口
    ├── 双串口支持
    │   ├── UART1 (调试串口)
    │   └── UART6 (串口屏通信)
    ├── 命令解析
    │   ├── parse_uart_command()      # UART1命令解析
    │   └── parse_serial_lcd_command() # UART6(串口屏)命令解析
    ├── 串口屏命令集
    │   ├── S2 → base2_function      # 基础功能2
    │   ├── S3 → base3_function      # 基础功能3
    │   ├── S4 → base4_function      # 基础功能4
    │   ├── S5 → improve1 (扫频重建)  # 改进功能1
    │   ├── S6 → improve2 (信号重建)  # 改进功能2
    │   └── I2/I3/I4 → 页面切换
    └── 标志位管理
        ├── g_sweep_reconstruction_trigger   # S5扫频重建触发
        ├── g_signal_reconstruction_trigger  # S6信号重建触发
        └── g_signal_reconstruction_active   # 信号重建激活状态
```

## 数据流与控制流

```
用户交互层:
┌─────────────┐    ┌─────────────┐
│  串口屏界面  │    │   PC调试    │
│   S2/S3/S4  │    │   串口命令   │
│   S5/S6命令  │    │            │
└─────────────┘    └─────────────┘
       │                   │
       └───────┬───────────┘
               ▼
通信处理层:
┌─────────────────────────────────┐
│         UART模块                │
│  ┌─────────────┐ ┌─────────────┐ │
│  │   UART6    │ │   UART1    │ │
│  │  (串口屏)   │ │  (调试)     │ │
│  └─────────────┘ └─────────────┘ │
└─────────────────────────────────┘
               │
               ▼
功能控制层:
┌─────────────────────────────────┐
│         Parser模块              │
│    功能模式选择与参数解析        │
└─────────────────────────────────┘
               │
               ▼
信号处理层:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   ADC模块   │    │ Frequency   │    │   DAC模块   │
│  数据采集   │───▶│  频域分析   │───▶│  信号合成   │
│  模式控制   │    │  滤波辨识   │    │  DDS输出   │
└─────────────┘    └─────────────┘    └─────────────┘
               │                           │
               ▼                           ▼
硬件接口层:
┌─────────────┐                   ┌─────────────┐
│  ADC1/ADC2  │                   │ DAC1/AD9954 │
│  双通道采样  │                   │  波形输出   │
└─────────────┘                   └─────────────┘
```

## 核心功能流程

### 扫频重建流程 (S5 命令)

```
S5命令 → g_sweep_reconstruction_trigger=1 → ADC_MODE_SWEEP
├── 1. 设置起始频率 (100Hz)
├── 2. AD9954输出激励信号
├── 3. ADC双通道采样 (输入+输出)
├── 4. FFT分析计算传递函数 H(jω)
├── 5. 频率递增 (100Hz步进)
├── 6. 重复步骤2-5 (491个频点)
└── 7. 滤波器辨识 → 传递函数系数
```

### 信号重建流程 (S6 命令)

```
S6命令 → g_signal_reconstruction_trigger=1 → ADC_MODE_RECONSTRUCT
├── 1. 分析输入信号谐波成分
├── 2. 选择最佳分析方法
├── 3. 基于传递函数预测输出
├── 4. 重建输出波形
├── 5. DAC+DDS输出合成信号
└── 6. Timer6定时重复 (可选)
```

## 主要数据结构

```
├── fundamental_result_t    # 基波分析结果
├── harmonic_component_t    # 谐波分量
├── ContinuousTransferFunction # 传递函数
├── DDS_Generator_t         # DDS生成器
└── time_domain_result_t    # 时域分析结果
```

## 硬件接口

```
外设接口
├── ADC1/ADC2 (14位, 双通道同步采样)
├── DAC1 (DDS波形输出)
├── UART1 (调试串口, 115200)
├── UART6 (串口屏通信)
├── SPI1 (AD9954外部DDS)
├── I2C1 (INA226功率监控)
├── TIM3 (ADC触发定时器)
├── TIM4 (DAC更新定时器)
├── TIM6 (信号重建定时器)
└── GPIO (按键输入, LED指示)
```

```

---

这份文档总结了STM32-accelerator项目的核心架构，突出了ADC、DAC、Frequency、Parser、UART五个主要模块的功能和相互关系，以及RTOS任务的分工和数据流向。

## 总结

本项目是一个基于STM32H7的多功能信号处理系统，具有以下核心特点：

### 主要功能
- **双ADC同步采样**：实现高精度信号采集
- **滤波器辨识**：通过扫频分析识别滤波器传递函数
- **信号重建**：基于谐波分析重建输出波形
- **多模式DDS**：支持多种波形生成和输出

### 技术特点
- **实时性**：基于FreeRTOS的多任务调度
- **高精度**：14位ADC分辨率，4096点FFT分析
- **灵活性**：支持多种窗函数和插值算法
- **可扩展性**：模块化设计，易于功能扩展

### 应用场景
- 滤波器特性测试与分析
- 信号处理算法验证
- 自动化测试设备
- 教学实验平台
```

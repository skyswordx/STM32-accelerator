# STM32 Accelerator 系统设计文档

## 1. 系统架构概述

本系统基于STM32微控制器，主要实现以下功能：
1. ADC数据采集与处理
2. DDS信号生成
3. DAC波形输出
4. 串口通信控制
5. LCR测量功能

## 2. 全局变量定义

根据系统需求，需要定义以下全局变量用于参数控制：

### 2.1 DDS相关参数
```c
// DDS期望频率 (Hz)
extern uint32_t g_desired_dds_frequency;

// DDS期望类型 (AD9833, AD9954等)
extern uint8_t g_desired_dds_type;

// DDS期望相位 (度)
extern uint32_t g_desired_dds_phase;

// DDS期望幅度
extern uint32_t g_desired_dds_amplitude;
```

### 2.2 ADC相关参数
```c
// ADC期望采样率 (Hz)
extern uint32_t g_desired_ADC_sample_rate_Hz;
```

### 2.3 DAC相关参数
```c
// DAC输出期望波形 (正弦波、方波、三角波等)
extern uint8_t g_desired_DAC_output_waveform;

// DAC输出期望频率 (Hz)
extern uint32_t g_desired_DAC_output_frequency;

// DAC输出期望幅度 (电压值)
extern float32_t g_desired_DAC_single_output_amplitude;
```

### 2.4 继电器控制参数
```c
// 控制哪个继电器
extern uint8_t g_desired_switch2which_relay;
```

### 2.5 功能状态参数
```c
// 功能状态枚举
typedef enum {
    LCR_STATE = 0,      // LCR表测量功能
    SPECTRUM_STATE,     // 频谱分析功能
    TIME_STATE,         // 时域分析功能
    DIY_STATE           // 自定义功能
} function_state_t;

// 期望功能状态
extern function_state_t g_desired_function_state;
```

### 2.6 频谱数据缓冲区
```c
// ADC1频谱数据缓冲区
extern float32_t g_adc1_spectrum_data[FFT_LENGTH / 2];

// ADC2频谱数据缓冲区
extern float32_t g_adc2_spectrum_data[FFT_LENGTH / 2];
```

## 3. 全局变量实现说明

### 3.1 头文件声明
需要创建 `my_parameter_config.h` 头文件，在其中声明所有全局变量：

```c
#ifndef MY_PARAMETER_CONFIG_H
#define MY_PARAMETER_CONFIG_H

#include "arm_math.h"
#include "my_freq_config.h"

// DDS相关参数
extern uint32_t g_desired_dds_frequency;
extern uint8_t g_desired_dds_type;
extern uint32_t g_desired_dds_phase;
extern uint32_t g_desired_dds_amplitude;

// ADC相关参数
extern uint32_t g_desired_ADC_sample_rate_Hz;

// DAC相关参数
extern uint8_t g_desired_DAC_output_waveform;
extern uint32_t g_desired_DAC_output_frequency;
extern float32_t g_desired_DAC_single_output_amplitude;

// 继电器控制参数
extern uint8_t g_desired_switch2which_relay;

// 功能状态参数
typedef enum {
    LCR_STATE = 0,
    SPECTRUM_STATE,
    TIME_STATE,
    DIY_STATE
} function_state_t;

extern function_state_t g_desired_function_state;

// 频谱数据缓冲区
extern float32_t g_adc1_spectrum_data[FFT_LENGTH / 2];
extern float32_t g_adc2_spectrum_data[FFT_LENGTH / 2];

#endif // MY_PARAMETER_CONFIG_H
```

### 3.2 源文件定义
需要创建 `my_parameter_config.c` 源文件，在其中定义所有全局变量：

```c
#include "my_parameter_config.h"

// DDS相关参数
uint32_t g_desired_dds_frequency = 1000;        // 默认1kHz
uint8_t g_desired_dds_type = 1;                 // 默认AD9833
uint32_t g_desired_dds_phase = 0;               // 默认0度
uint32_t g_desired_dds_amplitude = 16383;       // 默认最大幅度

// ADC相关参数
uint32_t g_desired_ADC_sample_rate_Hz = 2000000; // 默认2MHz

// DAC相关参数
uint8_t g_desired_DAC_output_waveform = 0;      // 默认正弦波
uint32_t g_desired_DAC_output_frequency = 1000; // 默认1kHz
float32_t g_desired_DAC_single_output_amplitude = 0.7f; // 默认0.7V

// 继电器控制参数
uint8_t g_desired_switch2which_relay = 0;       // 默认继电器0

// 功能状态参数
function_state_t g_desired_function_state = LCR_STATE; // 默认LCR状态

// 频谱数据缓冲区
float32_t g_adc1_spectrum_data[FFT_LENGTH / 2] = {0};
float32_t g_adc2_spectrum_data[FFT_LENGTH / 2] = {0};
```

## 4. 串口通信协议设计

### 4.1 协议格式
采用 `CMD:PARAM:VALUE` 格式，使用回车符(0x0D)作为结束符。

### 4.2 命令定义

#### 4.2.1 设置参数命令 (SET)
用于设置系统参数：
- `SET:DDS_FREQ:value` - 设置DDS频率
- `SET:ADC_RATE:value` - 设置ADC采样率
- `SET:DDS_TYPE:value` - 设置DDS类型 (AD9833或AD9954)
- `SET:DDS_PHASE:value` - 设置DDS相位
- `SET:DDS_AMP:value` - 设置DDS幅度
- `SET:DAC_WAVE:value` - 设置DAC波形 (SINE, SQUARE, TRIANGLE)
- `SET:DAC_FREQ:value` - 设置DAC频率
- `SET:DAC_AMP:value` - 设置DAC幅度
- `SET:RELAY:value` - 设置继电器
- `SET:FUNC:value` - 设置功能状态 (LCR, SPECTRUM, TIME, DIY)

#### 4.2.2 查询命令 (GET)
用于查询系统状态：
- `GET:ALL` - 获取所有参数状态
- `GET:DDS_FREQ` - 获取DDS频率
- `GET:ADC_RATE` - 获取ADC采样率
- 等等...

### 4.3 参数值定义

#### 4.3.1 DDS类型
- `AD9833` - 对应值: 1
- `AD9954` - 对应值: 0

#### 4.3.2 DAC波形
- `SINE` - 正弦波
- `SQUARE` - 方波
- `TRIANGLE` - 三角波

#### 4.3.3 功能状态
- `LCR` - LCR测量功能
- `SPECTRUM` - 频谱分析功能
- `TIME` - 时域分析功能
- `DIY` - 自定义功能

## 5. 系统数据流设计

### 5.1 ADC数据流
ADC数据处理严格遵循以下流程：
1. 等待DMA传输完成标志位（DMA传输完成的中断会停止ADC并设置标志位）
2. 数据处理（FFT分析、频谱插值等）
3. 根据功能状态进行不同处理：
   - LCR_STATE: LCR表测量
   - SPECTRUM_STATE: 频谱分析并打印
   - TIME_STATE: 时域分析并打印
   - DIY_STATE: 自定义功能

### 5.2 UART数据流
1. 接收上位机发送的命令
2. 解析命令并更新相应全局变量
3. 根据命令类型执行相应操作

## 6. 功能模块设计

### 6.1 ADC模块
负责数据采集和处理，支持正常模式和扫频模式。

### 6.2 DDS模块
支持AD9833和AD9954两种DDS芯片，可设置频率、相位和幅度。

### 6.3 DAC模块
支持多种波形输出，可设置频率和幅度。

### 6.4 UART模块
实现串口通信协议，支持参数设置和状态查询。

### 6.5 按键模块
通过矩阵键盘实现本地控制功能。

## 7. 系统状态管理

系统通过功能状态参数 `g_desired_function_state` 控制不同工作模式：
- LCR_STATE: 执行LCR测量功能
- SPECTRUM_STATE: 执行频谱分析并打印结果
- TIME_STATE: 执行时域分析并打印结果
- DIY_STATE: 保留用于自定义功能开发

## 8. 频谱数据存储优化

为避免频谱数据覆盖问题，需要为每个ADC通道创建独立的数据缓冲区：
- `g_adc1_spectrum_data`: 存储ADC1通道的频谱数据
- `g_adc2_spectrum_data`: 存储ADC2通道的频谱数据

在频谱分析函数中，需要将结果分别存储到对应的缓冲区中，而不是使用共享的缓冲区。
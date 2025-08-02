# Frequency 频率测量模块

## 概述

Frequency模块是用于信号频率、幅值和相位测量的核心模块。该模块基于FFT算法实现，支持多种窗函数和频谱插值算法，以提高测量精度。

## 功能特性

### 1. 基本FFT分析
- 支持复数FFT和实数FFT两种模式
- 可配置的FIR滤波器预处理
- 直流分量自动去除

### 2. 窗函数支持
- **无窗函数** - 不应用任何窗函数
- **汉宁窗** - 通用窗函数，平衡频率分辨率和幅度精度
- **平顶窗** - 专为高精度幅度测量设计的窗函数

### 3. 频谱插值算法
- **无插值** - 使用FFT峰值点的频率和幅值
- **二次抛物线插值** - 在对数域进行抛物线拟合插值
- **汉宁窗专用插值** - 基于汉宁窗特性的专用插值算法

### 4. 高级频率估计算法
- **Quinn频率估计算法** - 可选的高精度频率估计算法

## API接口

### 主要函数

```c
/**
 * @brief 应用复数FFT算法进行频谱分析
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param window_type 窗函数类型 (WINDOW_NONE, WINDOW_HANNING, WINDOW_FLAT_TOP)
 * @param interpolation_mode 频谱插值模式
 */
void my_armcfft32_apply(float32_t* adc_input, fundamental_result_t* result, 
                       uint8_t enable_fir, window_type_t window_type, 
                       spectral_interpolation_mode_t interpolation_mode);

/**
 * @brief 应用实数FFT算法进行频谱分析
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param window_type 窗函数类型 (WINDOW_NONE, WINDOW_HANNING, WINDOW_FLAT_TOP)
 * @param interpolation_mode 频谱插值模式
 */
void my_armrfft32_apply(float32_t* adc_input, fundamental_result_t* result, 
                       uint8_t enable_fir, window_type_t window_type, 
                       spectral_interpolation_mode_t interpolation_mode);
```

### 向后兼容接口

为了保持向后兼容性，模块还提供了使用旧参数的接口：

```c
/**
 * @brief 向后兼容的复数FFT应用函数
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param enable_window 是否启用窗函数 (1=启用, 0=禁用，启用时使用汉宁窗)
 * @param interpolation_mode 频谱插值模式
 */
void my_armcfft32_apply_old(float32_t* adc_input, fundamental_result_t* result, 
                           uint8_t enable_fir, uint8_t enable_window, 
                           spectral_interpolation_mode_t interpolation_mode);

/**
 * @brief 向后兼容的实数FFT应用函数
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param enable_window 是否启用窗函数 (1=启用, 0=禁用，启用时使用汉宁窗)
 * @param interpolation_mode 频谱插值模式
 */
void my_armrfft32_apply_old(float32_t* adc_input, fundamental_result_t* result, 
                           uint8_t enable_fir, uint8_t enable_window, 
                           spectral_interpolation_mode_t interpolation_mode);
```

## 窗函数选择指南

### 无窗函数 (WINDOW_NONE)
- **适用场景**: 信号频率恰好与FFT频率栅格对齐时
- **优点**: 无幅度衰减，保持原始信号幅度
- **缺点**: 频谱泄漏严重，仅在理想条件下准确

### 汉宁窗 (WINDOW_HANNING)
- **适用场景**: 通用测量，需要平衡频率分辨率和幅度精度
- **优点**: 显著减少频谱泄漏，良好的综合性能
- **缺点**: 幅度测量精度约±1.5dB

### 平顶窗 (WINDOW_FLAT_TOP)
- **适用场景**: 需要高精度幅度测量的应用
- **优点**: 极低的幅度波动(<0.1dB)，适合校准和精确测量
- **缺点**: 频率分辨率较低，主瓣较宽

## 使用示例

### 基本使用

```c
#include "my_freq_config.h"

// 假设adc_data是包含采样数据的数组
float32_t adc_data[FFT_LENGTH];
fundamental_result_t result;

// 使用平顶窗进行高精度幅度测量
my_armcfft32_apply(adc_data, &result, 1, WINDOW_FLAT_TOP, INTERPOLATION_HANNING_SPECIAL);

printf("频率: %d Hz\n", result.fundamental_frequency);
printf("峰峰值: %.6f V\n", result.fundamental_vpp);
printf("有效值: %.6f V\n", result.fundamental_vrms);
printf("相位角: %.2f 度\n", result.fundamental_phase_angle);
```

### 向后兼容使用

```c
// 使用旧接口（启用FIR滤波器和汉宁窗）
my_armcfft32_apply_old(adc_data, &result, 1, 1, INTERPOLATION_HANNING_SPECIAL);
```

## 配置参数

### FFT长度
```c
#define FFT_LENGTH (4096) // FFT点数
```

### FIR滤波器
```c
#define FIR_ORDER   100
#define NUM_TAPS    (FIR_ORDER + 1) // 滤波器抽头数
```

### 窗函数补偿系数
```c
#define HANNING_WINDOW_FACTOR 2.0f   // 汉宁窗补偿系数
#define FLAT_TOP_WINDOW_FACTOR 0.216f // 平顶窗补偿系数
```

## 测试验证

模块包含测试程序 `test_flat_top_window.c`，用于验证窗函数功能的正确性。

## 注意事项

1. 窗函数长度必须与FFT长度一致
2. 平顶窗适合幅度测量，但频率分辨率较低
3. 在选择窗函数时需要根据应用需求权衡精度和分辨率
4. 不同窗函数的计算开销略有差异，平顶窗的生成需要更多的三角函数计算



我现在需要修改一下触发 g_signal_reconstruction_trigger，然后进入 adc_task 中的(g_signal_reconstruction_trigger == 1) 分支的逻辑
1. 第一次的触发源还是和原来一样，需要解析串口屏的 "S6" 命令才能开始第一次触发（在my_uart_parser.c中解析 "S6" 命令），这个触发源是由串口屏的按钮触发的
2. 在第一次触发ADC采样，然后根据已经存下来的幅相特性算出波形，然后成功设置软件的DDS-DAC输出之后，后续的触发就可以由 timer6 的定时中断修改 g_signal_reconstruction_trigger 触发
3. 每一次触发的流程逻辑是每一次触发都要经过 ADC 采样一次数据，根据幅相特性算出float的g_reconstructed_waveform，并且这个float的类型还要转换成uint16并且归一化到4095，最终更新 g_reconstructed_waveform_uint16
4. 由于一旦进入上一次触发后，DAC的DMA就会不断地搬运 g_reconstructed_waveform_uint16 的数据到DAC输出，所以每次触发都要重新计算 g_reconstructed_waveform 和 g_reconstructed_waveform_uint16以便更新数据，但是，为了避免在每次更新时，由于更新数据导致DAC输出的波形出现突变，我们可以设置一个类似 pingpongs 双缓冲的机制，使用两个缓冲区交替更新数据。
5. 具体的timer6中断函数在main函数中已经有了，注意不要重复定义，开启定时器6的中断使用 `HAL_TIM_Base_Start_IT(&htim6);`，关闭定时器6的中断使用 `HAL_TIM_Base_Stop_IT(&htim6);`


帮我设计合理的标志位，补充这个/* 帮我在uart6接收到 "S6" 时调用 */的实现，在void parse_serial_lcd_command(char* cmd)进行实现
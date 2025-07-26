# 正弦波时域峰值和边沿检测模块使用说明

## 1. 模块概述

本模块用于在时域上对正弦波输入ADC采样得到的数组进行处理，检测输入正弦波的峰值以及正弦波的边沿（上升沿+下降沿）。该模块实现了高精度的峰值检测和边沿检测，以满足精密测量的需求。

## 2. 功能特性

- 高精度峰值检测（支持抛物线插值）
- 高精度边沿检测（支持线性插值）
- 可配置的移动平均滤波器
- 信号中点自动计算
- 亚采样点精度的位置检测

## 3. 编译说明

1. 确保已将以下文件添加到项目中：
   - `Module/SineDetectTiming/my_sine_detect.h`
   - `Module/SineDetectTiming/my_sine_detect.c`

2. 确保项目已包含CMSIS-DSP库。

3. 编译项目。

## 4. 使用方法

### 4.1 启用模块

在`Module/ADC/my_adc_task.c`文件中，有一个全局标志`g_sine_detect_enabled`用于控制模块的启用/禁用。默认情况下，该标志被设置为0（禁用）。

要启用模块，请将`g_sine_detect_enabled`设置为1：
```c
g_sine_detect_enabled = 1; // 启用正弦波检测模块
```

### 4.2 配置模块

模块的配置在`Module/ADC/my_adc_task.c`文件中的ADC正常模式处理部分进行。默认配置如下：
```c
sine_detect_config_t config;
config.sample_rate = g_ADC_SAMPLE_RATE_Hz;
config.data_length = ADC_SAMPLE_SIZE;
config.enable_filter = 1; // 启用滤波
config.filter_length = 5; // 滤波器长度为5
```

您可以根据需要修改这些配置参数。

### 4.3 查看结果

当模块启用时，检测结果将通过printf函数输出到串口。输出内容包括：
- 峰值数量
- 边沿数量
- 信号中点
- 前10个峰值的详细信息
- 前10个边沿的详细信息

## 5. 测试说明

1. 编译并下载程序到目标设备。

2. 确保ADC输入端有正弦波信号。

3. 按下GPIOC_PIN_1按键，启动ADC采样和处理。

4. 观察串口输出，检查是否正确显示了峰值和边沿检测结果。

5. 可以通过修改`g_sine_detect_enabled`标志来启用或禁用模块，验证模块不会影响原有功能。

## 6. 注意事项

1. 模块默认处理ADC1通道的数据(`g_adc1_data_8bit`)，如需处理ADC2通道的数据，请修改`Module/ADC/my_adc_task.c`中的相关代码。

2. 模块使用了动态内存分配(`malloc`和`free`)，请确保系统中有足够的内存。

3. 模块的精度受到ADC采样率和信号质量的影响，高采样率和高质量信号有助于提高检测精度。
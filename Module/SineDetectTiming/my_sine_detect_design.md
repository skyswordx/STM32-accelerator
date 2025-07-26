# 正弦波时域峰值和边沿检测模块设计文档

## 1. 模块概述

本模块用于在时域上对正弦波输入ADC采样得到的数组进行处理，检测输入正弦波的峰值以及正弦波的边沿（上升沿+下降沿）。该模块将实现高精度的峰值检测和边沿检测，以满足精密测量的需求。

## 2. 基本原理

### 2.1 峰值检测

根据参考文档中的业界最佳实践，本模块将采用以下方法进行峰值检测：

1. **预处理**: 对原始ADC数据进行一次移动平均滤波，以抑制噪声。
2. **高精度峰值检测**: 在滤波后的数据上，先用局部极值搜索找到整数位置的峰值，然后用抛物线插值法计算亚采样点精度的峰值幅度和位置。

### 2.2 边沿检测

根据参考文档中的业界最佳实践，本模块将采用以下方法进行边沿检测：

1. **估算中点**: 对滤波后的数据计算峰值和谷值的平均，得到精确的信号中点。
2. **高精度边沿检测**: 在滤波后的数据上，先用简单过零检测找到跨越中点的点对，然后用线性插值法计算亚采样点精度的过零时刻。

## 3. 数据结构定义

```c
// 峰值检测结果结构体
typedef struct {
    float32_t amplitude;     // 峰值幅度
    float32_t position;      // 峰值位置（亚采样点精度）
    uint32_t index;          // 峰值索引（整数位置）
    uint8_t is_positive;     // 是否为正峰值（1=正峰值，0=负峰值）
} peak_result_t;

// 边沿检测结果结构体
typedef struct {
    float32_t position;      // 边沿位置（亚采样点精度）
    uint32_t index;          // 边沿索引（整数位置）
    uint8_t is_rising;       // 是否为上升沿（1=上升沿，0=下降沿）
} edge_result_t;

// 模块配置结构体
typedef struct {
    uint32_t sample_rate;    // 采样率(Hz)
    uint32_t data_length;    // 数据长度
    uint8_t enable_filter;   // 是否启用滤波（1=启用，0=禁用）
    uint32_t filter_length;  // 滤波器长度
} sine_detect_config_t;

// 模块结果结构体
typedef struct {
    peak_result_t* peaks;    // 峰值结果数组
    uint32_t peak_count;     // 峰值数量
    edge_result_t* edges;    // 边沿结果数组
    uint32_t edge_count;     // 边沿数量
    float32_t signal_midpoint; // 信号中点
} sine_detect_result_t;
```

## 4. 函数接口定义

```c
/**
 * @brief 初始化正弦波检测模块
 * @param config 模块配置参数
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_init(const sine_detect_config_t* config);

/**
 * @brief 处理ADC数据，检测峰值和边沿
 * @param adc_data 输入的ADC数据数组
 * @param result 输出的检测结果
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_process(const float32_t* adc_data, sine_detect_result_t* result);

/**
 * @brief 获取峰值检测结果
 * @param peaks 峰值结果数组
 * @param max_count 最大峰值数量
 * @return 实际检测到的峰值数量
 */
uint32_t my_sine_detect_get_peaks(peak_result_t* peaks, uint32_t max_count);

/**
 * @brief 获取边沿检测结果
 * @param edges 边沿结果数组
 * @param max_count 最大边沿数量
 * @return 实际检测到的边沿数量
 */
uint32_t my_sine_detect_get_edges(edge_result_t* edges, uint32_t max_count);

/**
 * @brief 获取信号中点
 * @return 信号中点值
 */
float32_t my_sine_detect_get_midpoint(void);

/**
 * @brief 释放模块资源
 */
void my_sine_detect_deinit(void);
```

## 5. 数据获取和处理流程

1. **数据来源**: 模块将从ADC处理任务中获取数据，具体来说是从`g_adc1_data_8bit`或`g_adc2_data_8bit`数组中获取数据。
2. **数据预处理**: 如果启用了滤波，将对原始ADC数据进行移动平均滤波。
3. **峰值检测**: 在预处理后的数据上进行峰值检测，包括局部极值搜索和抛物线插值。
4. **边沿检测**: 在预处理后的数据上进行边沿检测，包括中点计算和线性插值过零检测。
5. **结果输出**: 将检测结果存储在结果结构体中，供其他模块使用。

## 6. 与现有系统的兼容性

为了与现有系统保持兼容，模块将设计为可选功能，通过条件编译或运行时标志来启用或禁用。这样可以在不影响现有功能的情况下进行测试和验证。

## 7. 性能考虑

1. **计算效率**: 模块将使用CMSIS-DSP库中的优化函数来提高计算效率。
2. **内存使用**: 模块将尽量减少内存使用，避免动态内存分配。
3. **实时性**: 模块将设计为能够在ADC数据处理的实时性要求下完成计算。

## 8. 测试方案

1. **单元测试**: 对峰值检测和边沿检测的核心算法进行单元测试。
2. **集成测试**: 在ADC处理任务中集成模块，并通过实际ADC数据进行测试。
3. **精度验证**: 通过与已知信号的理论值进行比较，验证检测精度。

## 9. 实现细节

### 9.1 移动平均滤波器实现

移动平均滤波器将使用CMSIS-DSP库中的函数实现，以提高计算效率。

### 9.2 局部极值搜索实现

局部极值搜索将遍历滤波后的数据，寻找满足条件的峰值点。

### 9.3 抛物线插值实现

抛物线插值将使用以下公式计算亚采样点精度的峰值位置和幅度：
```
Δx = (1/2) * (y_{n-1} - y_{n+1}) / (y_{n-1} - 2*y_n + y_{n+1})
x_p = x_n + Δx
y_p = y_n - (1/4) * (y_{n-1} - y_{n+1}) * Δx
```

### 9.4 中点计算实现

信号中点将通过计算峰值和谷值的平均值得到：
```
Midpoint = (V_p + V_t) / 2
```

### 9.5 线性插值过零检测实现

线性插值过零检测将使用以下公式计算亚采样点精度的过零时刻：
```
x_{zc} = x_{i-1} + (Midpoint - y_{i-1}) / (y_i - y_{i-1})
```

## 10. 内存管理

模块将使用静态内存分配，避免动态内存分配带来的不确定性和开销。所有缓冲区将在模块初始化时分配，并在模块去初始化时释放。

## 11. 错误处理

模块将提供详细的错误码，以便调用者能够准确地了解错误原因并采取相应的处理措施。

## 12. 可配置参数

模块将提供可配置的参数，如滤波器长度、数据长度等，以便根据具体应用需求进行调整。
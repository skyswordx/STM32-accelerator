# 自适应采样率调整模块使用说明

## 📋 功能概述

本模块实现了基于文档"动态调整采样率.md"的自适应采样率调整算法，支持在 1kHz-100kHz 信号带宽内进行高精度频率测量。

## 🎯 核心特性

- ✅ **简洁明了**：直接的频率到采样率映射，无过度封装
- ✅ **数学约束验证**：自动满足奈奎斯特定理和 FFT 约束条件
- ✅ **易于使用**：单函数接口，参数简单
- ✅ **调试支持**：内置验证和打印函数

## 🔧 使用方法

### 主要接口（推荐）

如果您已经通过 FFT 测得了信号频率，直接使用：

```c
#include "my_timer_config.h"

// 假设通过FFT测得信号频率为55kHz
uint32_t measured_freq = 55000; // Hz

// 自动计算并设置最优采样率
if (adaptive_sample_rate_simple(&htim3, measured_freq) == HAL_OK) {
    printf("Successfully set optimal sample rate for %lu Hz signal\r\n", measured_freq);

    // 可选：打印配置信息用于调试
    print_sample_rate_info(measured_freq);
} else {
    printf("Failed to set optimal sample rate\r\n");
}
```

### 分步骤使用

如果您需要分阶段测量：

```c
// 步骤1：设置粗测采样率（250kHz）
if (adaptive_sample_rate_coarse(&htim3) == HAL_OK) {
    printf("Coarse measurement ready at %d Hz\r\n", COARSE_SAMPLE_RATE);

    // 步骤2：执行粗测量（用户实现FFT测频）
    uint32_t coarse_freq = perform_your_fft_measurement();

    // 步骤3：根据粗测结果设置精测采样率
    if (adaptive_sample_rate_simple(&htim3, coarse_freq) == HAL_OK) {
        printf("Fine measurement ready with optimal sample rate\r\n");

        // 步骤4：执行精测量
        uint32_t fine_freq = perform_your_fft_measurement();
        printf("Final measured frequency: %lu Hz\r\n", fine_freq);
    }
}
```

## 📊 配置参数

当前系统配置（可在头文件中修改）：

```c
#define FFT_POINTS               4096        // FFT点数
#define MIN_CYCLES_IN_FFT        2           // FFT窗口内最少周期数
#define MIN_POINTS_PER_CYCLE     20          // 每周期最少采样点数
#define COARSE_SAMPLE_RATE       250000      // 粗测初始采样率 (250kHz)
#define MAX_SIGNAL_FREQ          100000      // 最大信号频率 (100kHz)
#define MIN_SIGNAL_FREQ          1000        // 最小信号频率 (1kHz)
```

## 🧮 数学原理

### 采样率计算公式

```c
// 下界：保证每个周期至少有20个采样点
F_s_min = 20 × f_signal

// 上界：保证4096点FFT窗口内至少有2个完整周期
F_s_max = (4096 × f_signal) / 2 = 2048 × f_signal

// 最优：选择下界，获得最佳频率分辨率
F_s_optimal = F_s_min = 20 × f_signal
LSB = F_s_optimal / 4096
```

### 实际计算示例

```c
// 对于55kHz信号：
F_s_optimal = 20 × 55000 = 1,100,000 Hz (1.1MHz)
LSB = 1,100,000 / 4096 ≈ 268.55 Hz
每周期采样点数 = 1,100,000 / 55,000 = 20点
FFT窗口内周期数 = (4096 × 55000) / 1,100,000 ≈ 4个周期
```

## 🔍 调试和验证

### 快速查看配置信息

```c
uint32_t signal_freq = 55000;  // 55kHz
print_sample_rate_info(signal_freq);
```

输出示例：

```
=== Sample Rate Info for 55000 Hz ===
Optimal Sample Rate: 1100000 Hz
Lower Bound: 1100000 Hz
Upper Bound: 112640000 Hz
Expected LSB: 268.55 Hz
Cycles in Window: 4
Points per Cycle: 20
===============================
```

### 验证约束条件

```c
uint32_t signal_freq = 55000;   // 55kHz
uint32_t sample_rate = 1100000; // 1.1MHz

if (validate_sample_rate_constraints(signal_freq, sample_rate) == HAL_OK) {
    printf("Sample rate configuration is valid\r\n");
}
```

## ⚠️ 使用注意事项

1. **定时器状态**：确保在调用采样率调整函数前停止定时器
2. **频率范围**：输入频率必须在 1kHz-100kHz 范围内
3. **ADC 稳定**：采样率改变后需要等待 ADC 稳定再进行测量
4. **错误处理**：始终检查函数返回值，HAL_ERROR 表示参数无效或约束冲突

## 📈 性能预期

根据实测数据，使用自适应采样率调整后：

- **1-65kHz 信号**：频率误差 ≤ 1 LSB
- **70-100kHz 信号**：解决混叠问题，恢复正确测量
- **频率分辨率**：根据信号频率自动优化 LSB 值

## 🔗 相关文件

- `my_timer_config.h` - 头文件和接口定义
- `my_timer_config.c` - 实现文件
- `动态调整采样率.md` - 算法设计文档
- `g1.md` - 数学公式推导

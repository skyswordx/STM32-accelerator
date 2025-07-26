# Quinn频率估计算法模块

## 简介

Quinn频率估计算法模块是一个用于提高FFT频率估计精度的嵌入式软件模块。该模块实现了Quinn算法，可以显著提高频率估计的精度，特别适用于需要高精度频率测量的应用场景。

## 功能特点

- **高精度频率估计**：基于FFT峰值邻近点信息，提供比直接FFT估计更精确的频率值
- **易于集成**：模块化设计，易于集成到现有的频率分析系统中
- **可选启用**：支持编译时和运行时控制，可根据需要启用或禁用
- **结果可信度评估**：提供结果可信度评估机制，确保估计结果的可靠性
- **低计算开销**：算法简单高效，适合在资源受限的嵌入式系统中使用

## 文件结构

```
Quinn/
├── my_quinn_config.h     # 模块头文件，包含接口定义和配置参数
├── my_quinn_config.c     # 模块源文件，实现Quinn算法核心功能
├── my_quinn_design.md    # 详细设计文档
├── test_quinn.c          # 测试程序
└── README.md            # 本说明文件
```

## 使用方法

### 1. 集成到项目中

1. 将Quinn文件夹复制到项目的Module目录下
2. 在编译选项中添加`-DENABLE_QUINN_FREQUENCY_ESTIMATION`宏定义
3. 确保项目中包含了必要的头文件路径

### 2. 启用Quinn算法

Quinn算法支持两种启用方式：

#### 编译时启用
在编译时添加宏定义：
```bash
gcc -DENABLE_QUINN_FREQUENCY_ESTIMATION ...
```

或者在代码中添加：
```c
#define ENABLE_QUINN_FREQUENCY_ESTIMATION
```

#### 运行时控制
通过设置全局变量控制算法的启用/禁用：
```c
// 启用Quinn模块
g_quinn_module_enabled = 1;

// 禁用Quinn模块
g_quinn_module_enabled = 0;
```

### 3. 在Frequency模块中使用

Quinn模块已经集成到Frequency模块中，在`my_armcfft32_apply`和`my_armrfft32_apply`函数中会自动调用Quinn算法（如果已启用）。

### 4. 直接调用API

如果需要直接调用Quinn算法，可以使用以下API：

```c
#include "my_quinn_config.h"

// 执行Quinn频率估计算法
quinn_frequency_result_t quinn_result;
int status = my_quinn_process(fft_data, peak_index, &quinn_result);

if (status == MY_QUINN_SUCCESS && quinn_result.validity == 1) {
    // 使用精确频率估计
    float32_t precise_frequency = quinn_result.frequency;
    printf("精确频率: %.2f Hz\n", precise_frequency);
}
```

## API接口

### 核心函数

```c
int my_quinn_process(const float32_t* fft_data, uint16_t peak_index, quinn_frequency_result_t* result);
```
执行Quinn频率估计算法

### 辅助函数

```c
void my_quinn_get_result(quinn_frequency_result_t* result);
uint8_t my_quinn_is_result_valid(void);
float32_t my_quinn_get_confidence(void);
```

## 配置参数

在`my_quinn_config.h`中可以调整以下参数：

- `QUINN_VALIDITY_THRESHOLD`：结果有效性阈值
- `QUINN_CONFIDENCE_THRESHOLD`：结果可信度阈值

## 测试

提供了测试程序`test_quinn.c`，可以验证Quinn算法的正确性和精度。

编译并运行测试程序：
```bash
gcc -DENABLE_QUINN_FREQUENCY_ESTIMATION test_quinn.c my_quinn_config.c -lm -o test_quinn
./test_quinn
```

## 注意事项

1. Quinn算法要求输入的FFT数据为复数格式
2. 峰值点不能位于频谱的边缘位置（索引0或N/2-1）
3. 算法对噪声较为敏感，在低信噪比环境下可能失效
4. 确保系统中定义了`g_ADC_SAMPLE_RATE_Hz`全局变量，表示采样率

## 版本信息

当前版本：v1.0.0

## 许可证

请参考项目主许可证文件。
# AD5933 HAL驱动说明

## 概述

这个驱动程序是基于STM32 HAL库的AD5933阻抗分析仪芯片驱动，从原来的STM32标准库版本转换而来。主要特点包括：

- 使用STM32 HAL I2C库进行通信
- 支持阻抗测量和温度测量
- 提供完整的频率扫描功能
- 包含错误处理和状态检查
- 模块化设计，易于集成

## 文件结构

```
Drivers/AD5933/
├── AD5933_HAL.h          # 头文件，包含函数声明和数据结构
├── AD5933_HAL.c          # 主驱动文件
├── AD5933_HAL_example.c  # 使用示例
└── README.md             # 本说明文件
```

## 主要改进

### 1. I2C通信接口改进
- **原版本**: 使用标准库的GPIO模拟I2C
- **HAL版本**: 使用HAL库的硬件I2C，提供更好的稳定性和性能

### 2. 错误处理
- 所有函数返回`HAL_StatusTypeDef`状态码
- 提供详细的错误检查和状态验证

### 3. 配置管理
- 使用配置结构体统一管理设备参数
- 支持运行时配置修改

### 4. 功能增强
- 16位数据读写函数
- 设备就绪检测
- 完整的温度测量功能

## 使用方法

### 1. 硬件连接

```
STM32     AD5933
------    ------
PB6   --> SCL
PB7   --> SDA
VCC   --> 3.3V
GND   --> GND
```

### 2. CubeMX配置

1. 在CubeMX中启用I2C1（或其他I2C端口）
2. 配置I2C参数：
   - I2C Speed Mode: Standard Mode
   - I2C Clock Speed: 100 kHz
   - Rise Time: 1000ns
   - Fall Time: 300ns

### 3. 代码集成

#### 3.1 包含头文件
```c
#include "AD5933_HAL.h"
```

#### 3.2 初始化
```c
// 在main函数中初始化
HAL_StatusTypeDef status = AD5933_Init(&hi2c1);
if (status != HAL_OK) {
    // 处理初始化错误
}
```

#### 3.3 基本使用
```c
ImpeType impedance_result;

// 设置测量参数
AD5933_SetConfig(0x00, 0x01, 0x00);  // 内部时钟, x1增益, 2Vpp
AD5933_SetFrequency(10000, 100, 10);  // 10kHz起始, 100Hz增量, 10点

// 初始化频率扫描
AD5933_FreInit(10000.0f, 100.0f);

// 执行测量
for (int i = 0; i < 10; i++) {
    AD5933_StartOnceTest(&impedance_result, 1);
    printf("阻抗: %.2f Ω\r\n", impedance_result.Impedance);
}
```

## API参考

### 初始化函数
- `AD5933_Init()` - 初始化AD5933设备
- `AD5933_FreInit()` - 初始化频率扫描参数

### 数据读写函数
- `AD5933_WriteByte()` - 写入单字节
- `AD5933_ReadByte()` - 读取单字节
- `AD5933_WriteWord()` - 写入16位数据
- `AD5933_ReadWord()` - 读取16位数据

### 测量函数
- `AD5933_StartTest()` - 开始测试
- `AD5933_ReadImpedance()` - 读取阻抗数据
- `AD5933_StartOnceTest()` - 执行一次完整测量
- `AD5933_Temperature_Test()` - 测量温度

### 配置函数
- `AD5933_SetConfig()` - 设置基本配置
- `AD5933_SetFrequency()` - 设置频率参数
- `AD5933_SetSettlingTime()` - 设置稳定时间

## 数据结构

### ImpeType - 阻抗测量结果
```c
typedef struct {
    int16_t Re;           // 实部
    int16_t Im;           // 虚部
    float Impedance;      // 阻抗幅值 (Ω)
    float Phase;          // 相位角 (弧度)
} ImpeType;
```

### AD5933_Config - 设备配置
```c
typedef struct {
    I2C_HandleTypeDef *hi2c;      // I2C句柄
    uint8_t device_addr;          // 设备地址
    uint8_t clk_source;           // 时钟源
    uint8_t gain;                 // 增益设置
    uint8_t output_range;         // 输出范围
    uint32_t start_freq;          // 起始频率
    uint32_t freq_increment;      // 频率增量
    uint16_t num_increments;      // 扫描点数
    uint16_t settling_cycles;     // 稳定周期
} AD5933_Config;
```

## 常见问题

### 1. 初始化失败
- 检查I2C连接
- 确认设备地址正确
- 检查电源供应

### 2. 测量结果异常
- 确保外部电路连接正确
- 检查激励信号设置
- 验证频率范围设置

### 3. I2C通信错误
- 检查SCL/SDA引脚配置
- 确认I2C时钟频率设置
- 检查上拉电阻

## 注意事项

1. **I2C配置**: 确保I2C时钟频率不超过400kHz
2. **延时设置**: 某些操作需要适当的延时，特别是复位后
3. **错误处理**: 建议检查所有函数的返回值
4. **浮点运算**: 代码中使用了浮点运算，确保工程中启用了FPU（如果可用）

## 示例代码

完整的使用示例请参考 `AD5933_HAL_example.c` 文件，其中包含：
- 基本测量示例
- 单点测量函数
- 自定义频率扫描
- 校准程序示例

## 版本历史

- v1.0: 从STM32标准库版本转换为HAL版本
- 增加了完整的错误处理
- 优化了代码结构和可读性

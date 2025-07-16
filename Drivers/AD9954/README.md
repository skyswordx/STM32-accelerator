# AD9954 HAL 驱动使### 4. 延时函数

- 原来: `delay_ms()` 和 `delay_us()`
- 现在: `HAL_Delay()` 和基于 SysTick 的`delay_us()`

### 5. 精确延时实现

提供了两种 delay_us 实现方式：

1. **简单循环延时** (`delay_us`)：占用资源少，精度依赖于系统时钟
2. **SysTick 延时** (`delay_us_systick`)：更精确，但占用资源更多

### 6. GPIO 初始化

## 概述

本驱动已重构为使用 STM32 HAL 库的版本，保持原有的软件 SPI 驱动逻辑不变，但替换了底层的 IO 操作方式。

## 主要改动

### 1. 头文件包含

- 原来: `#include "sys.h"` 和 `#include "delay.h"`
- 现在: `#include "main.h"`

### 2. GPIO 宏定义

- 原来: 使用寄存器操作的宏（如 `PAout(3)`）
- 现在: 使用 HAL API 的宏（如 `HAL_GPIO_WritePin()`）

### 3. 延时函数

- 原来: `delay_ms()` 和 `delay_us()`
- 现在: `HAL_Delay()` 和简单的循环延时

### 4. GPIO 初始化

- 原来: 在代码中使用寄存器直接配置 GPIO
- 现在: 需要在 CubeMX 中配置 GPIO，代码中只设置初始状态

## CubeMX GPIO 配置要求

请在 CubeMX 中按以下要求配置 GPIO：

### 输出引脚 (Push-Pull Output, GPIO_SPEED_FREQ_HIGH)

- PA2 -> PS1 (User Label: PS1)
- PA3 -> AD9954_CS (User Label: AD9954_CS)
- PA4 -> AD9954_SCLK (User Label: AD9954_SCLK)
- PA5 -> AD9954_SDIO (User Label: AD9954_SDIO)
- PA6 -> AD9954_RES (User Label: AD9954_RES)
- PA7 -> IOUPDATE (User Label: IOUPDATE)
- PB0 -> AD9954_PWR (User Label: AD9954_PWR)
- PB1 -> AD9954_IOSY (User Label: AD9954_IOSY)
- PB10 -> PS0 (User Label: PS0)
- PC0 -> AD9954_OSK (User Label: AD9954_OSK)

### 输入引脚 (Input Pull-Up)

- PA8 -> AD9954_SDO (User Label: AD9954_SDO)

## 使用方法

### 1. 编译配置

确保项目包含以下文件：

- `AD9954.h` - 头文件
- `AD9954.c` - 主要驱动实现
- `AD9954_delay.c` - 延时函数实现（可选）

### 2. 延时函数选择

根据需要选择合适的延时函数实现：

```c
// 方式1：使用简单循环延时（推荐）
void delay_us(uint32_t us);

// 方式2：使用SysTick延时（更精确）
void delay_us_systick(uint32_t us);
```

### 3. 初始化

```c
// 在CubeMX生成的代码中，GPIO已经被初始化
// 只需要调用AD9954的初始化函数
AD9954_Init();
```

### 4. 基本功能

```c
// 设置频率输出
AD9954_Set_Fre(1000000.0); // 1MHz

// 设置幅度
AD9954_Set_Amp(8192); // 约250mV峰峰值

// 设置相位
AD9954_Set_Phase(4096); // 90度相位
```

### 5. 高级功能

```c
// FSK调制
AD9954_SetFSK(1000000.0, 2000000.0, 3000000.0, 4000000.0, 8192);

// PSK调制
AD9954_SetPSK(0, 4096, 8192, 12288, 1000000.0, 8192);

// 线性扫频
AD9954_Set_LinearSweep(1000000.0, 10000000.0, 1000.0, 100, 1000.0, 100, No_Dwell);
```

## 注意事项

1. **GPIO 配置**: 必须在 CubeMX 中正确配置所有 GPIO 引脚
2. **延时精度**:
   - 使用`AD9954_delay_test.c`中的`delay_us_systick()`可获得更高精度
   - 简单的循环延时在不同优化级别下可能有差异
3. **编译**: 确保项目包含 HAL 库相关文件
4. **调试**: 如果遇到问题，可以使用示波器检查 SPI 时序

## 延时函数选择

根据您的需求选择合适的延时函数：

### 方案 1：简单循环延时（推荐）

```c
// 在您的项目中实现
void delay_us(uint32_t us) {
    for(uint32_t i = 0; i < us * (SystemCoreClock / 1000000 / 10); i++) {
        __asm volatile ("nop");
    }
}
```

### 方案 2：基于 SysTick 的精确延时

使用`AD9954_delay_test.c`中的`delay_us_systick()`函数。

### 方案 3：使用 HAL 库延时

对于不需要高精度的应用，可以使用`HAL_Delay(1)`替代微秒延时。

## 快速开始

1. **复制文件到项目**:

   - `AD9954.h`
   - `AD9954.c`
   - `AD9954_example.c`（可选）

2. **在 CubeMX 中配置 GPIO**（参考头文件中的说明）

3. **在代码中调用**:

   ```c
   #include "AD9954.h"

   int main(void) {
       HAL_Init();
       SystemClock_Config();
       MX_GPIO_Init();

       AD9954_Init();
       AD9954_Set_Fre(1000000.0);  // 1MHz

       while(1) {
           // 您的主循环代码
       }
   }
   ```

## 兼容性

- 支持 STM32H7 系列
- 使用 HAL 库版本
- 保持原有 API 接口不变

## 文件结构

```
Drivers/AD9954/
├── AD9954.h        # 头文件，包含宏定义和函数声明
├── AD9954.c        # 源文件，包含所有实现
└── README.md       # 本说明文档
```

# AD9554 CubeMX配置指导

## 概述
AD9554驱动库现已完全适配CubeMX自动生成的外设初始化代码。驱动库不再包含GPIO初始化代码，需要用户在CubeMX中手动配置相关引脚。

## GPIO引脚配置

### 必需的GPIO引脚
根据AD9554.h中的定义，需要配置以下引脚：

#### GPIOE引脚
- **PE7 (OSK)** - 输出移位键控，推挽输出，无上拉
- **PE9 (PS0)** - 配置文件选择位0，推挽输出，无上拉
- **PE11 (PS1)** - 配置文件选择位1，推挽输出，无上拉
- **PE13 (IOUPDATE)** - I/O更新信号，推挽输出，无上拉

#### GPIOF引脚
- **PF11 (SDO)** - 串行数据输出，输入，下拉
- **PF12 (CS)** - 片选信号，推挽输出，无上拉
- **PF14 (SCLK)** - 串行时钟，推挽输出，无上拉
- **PF15 (RES)** - 复位信号，推挽输出，无上拉

#### GPIOG引脚
- **PG0 (SDIO)** - 串行数据输入/输出，推挽输出，无上拉
- **PG1 (PWR)** - 功率控制，推挽输出，无上拉

## CubeMX配置步骤

### 1. 引脚配置
1. 打开CubeMX项目
2. 在Pinout & Configuration标签页中，找到上述引脚
3. 对于每个引脚，右键选择"GPIO_Output"（除PF11选择"GPIO_Input"）
4. 配置引脚属性：
   - **GPIO mode**: Output Push Pull（输出引脚）或 Input mode（PF11）
   - **GPIO Pull-up/Pull-down**: No pull-up and no pull-down（输出引脚）或 Pull-down（PF11）
   - **Maximum output speed**: High
   - **User Label**: 建议使用对应的功能名称（如OSK, PS0, CS等）

### 2. 时钟配置
1. 在Clock Configuration标签页中，确保相关GPIO端口的时钟已使能
2. 如果使用的是STM32H7系列，确保AHB4时钟正常

### 3. 代码生成
1. 在Project Manager中配置项目设置
2. 点击"GENERATE CODE"生成初始化代码
3. 生成的代码会自动包含GPIO初始化

## 使用示例

### 在main.c中使用AD9554驱动

```c
#include "main.h"
#include "AD9554.h"

// 延时函数实现（用户需要提供）
void AD9554_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

void AD9554_DelayUs(uint32_t us)
{
    // 简单的微秒延时实现
    uint32_t start = HAL_GetTick();
    while((HAL_GetTick() - start) < ((us + 999) / 1000));
}

int main(void)
{
    // HAL库初始化
    HAL_Init();
    
    // 系统时钟配置
    SystemClock_Config();
    
    // GPIO初始化（CubeMX自动生成）
    MX_GPIO_Init();
    
    // AD9554初始化
    AD9554_Init();
    
    // 设置频率为1MHz
    AD9554_SetFrequency(1000000.0);
    
    // 设置幅度
    AD9554_SetAmplitude(0x3FFF);
    
    // 设置相位
    AD9554_SetPhase(0x0000);
    
    while (1)
    {
        // 主循环
    }
}
```

## 引脚修改

如果需要使用不同的引脚，只需修改AD9554.h中的引脚定义：

```c
// 示例：将CS引脚改为PB12
#define AD9554_SPI_CS_PORT     GPIOB
#define AD9554_SPI_CS_PIN      GPIO_PIN_12
```

然后在CubeMX中重新配置对应的引脚。

## 注意事项

1. **引脚冲突**: 确保所选择的引脚没有被其他外设使用
2. **时钟使能**: CubeMX会自动生成时钟使能代码，无需手动配置
3. **延时函数**: 用户必须实现`AD9554_DelayMs`和`AD9554_DelayUs`函数
4. **初始化顺序**: 必须先调用`MX_GPIO_Init()`，再调用`AD9554_Init()`

## 常见问题

### Q: 为什么驱动库不再包含GPIO初始化代码？
A: 为了更好地与CubeMX集成，避免代码重复，让用户可以通过图形化界面配置引脚。

### Q: 如何验证引脚配置是否正确？
A: 可以在调用AD9554_Init()后，使用示波器检查各引脚的电平状态。

### Q: 可以使用硬件SPI吗？
A: 当前版本使用软件SPI，如需硬件SPI，需要修改驱动代码。

### Q: 如何处理引脚冲突？
A: 在CubeMX中选择不同的引脚，并修改AD9554.h中的对应定义。

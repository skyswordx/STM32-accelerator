# MSPM0G3507 DL 库 API 文档

## 目录

- [概述](#概述)
- [快速 API 参考表](#快速API参考表)
- [外设分类](#外设分类)
  - [1. GPIO（通用输入输出）](#1-gpio通用输入输出---dl_gpioh)
  - [2. ADC12（12 位模数转换器）](#2-adc1212位模数转换器---dl_adc12h)
  - [3. UART（通用异步收发传输器）](#3-uart通用异步收发传输器---dl_uarth)
  - [4. 定时器（Timer）](#4-定时器timer---dl_timerh)
  - [5. 其他外设模块](#5-其他外设模块)
  - [6. 系统级模块](#6-系统级模块)
  - [7. 通用定义](#7-通用定义---dl_commonh)
- [使用说明](#使用说明)
- [版本信息](#版本信息)

## 概述

本文档详细介绍了 MSPM0G3507 微控制器的 DL（Driver Library）库 API。这些 API 从 TI 官方 SDK 2.05.00.05 版本中提取，按外设功能进行分类整理。

## 快速 API 参考表

### 常用外设电源管理 API

| 外设  | 启用电源                 | 禁用电源                  | 检查电源状态                |
| ----- | ------------------------ | ------------------------- | --------------------------- |
| GPIO  | `DL_GPIO_enablePower()`  | `DL_GPIO_disablePower()`  | `DL_GPIO_isPowerEnabled()`  |
| ADC12 | `DL_ADC12_enablePower()` | `DL_ADC12_disablePower()` | `DL_ADC12_isPowerEnabled()` |
| UART  | `DL_UART_enablePower()`  | `DL_UART_disablePower()`  | `DL_UART_isPowerEnabled()`  |
| I2C   | `DL_I2C_enablePower()`   | `DL_I2C_disablePower()`   | `DL_I2C_isPowerEnabled()`   |
| SPI   | `DL_SPI_enablePower()`   | `DL_SPI_disablePower()`   | `DL_SPI_isPowerEnabled()`   |
| Timer | `DL_Timer_enablePower()` | `DL_Timer_disablePower()` | `DL_Timer_isPowerEnabled()` |

### 常用外设复位 API

| 外设  | 复位               | 检查复位状态         |
| ----- | ------------------ | -------------------- |
| GPIO  | `DL_GPIO_reset()`  | `DL_GPIO_isReset()`  |
| ADC12 | `DL_ADC12_reset()` | `DL_ADC12_isReset()` |
| UART  | `DL_UART_reset()`  | `DL_UART_isReset()`  |
| I2C   | `DL_I2C_reset()`   | `DL_I2C_isReset()`   |
| SPI   | `DL_SPI_reset()`   | `DL_SPI_isReset()`   |
| Timer | `DL_Timer_reset()` | `DL_Timer_isReset()` |

### 常用外设使能 API

| 外设  | 使能                           | 禁用                            | 检查使能状态                |
| ----- | ------------------------------ | ------------------------------- | --------------------------- |
| UART  | `DL_UART_enable()`             | `DL_UART_disable()`             | `DL_UART_isEnabled()`       |
| I2C   | `DL_I2C_enable()`              | `DL_I2C_disable()`              | `DL_I2C_isEnabled()`        |
| SPI   | `DL_SPI_enable()`              | `DL_SPI_disable()`              | `DL_SPI_isEnabled()`        |
| ADC12 | `DL_ADC12_enableConversions()` | `DL_ADC12_disableConversions()` | -                           |
| Timer | `DL_Timer_enableClock()`       | `DL_Timer_disableClock()`       | `DL_Timer_isClockEnabled()` |

### GPIO 快速操作 API

| 功能           | API 函数                                  |
| -------------- | ----------------------------------------- |
| 设置引脚高电平 | `DL_GPIO_setPins(gpio, pins)`             |
| 设置引脚低电平 | `DL_GPIO_clearPins(gpio, pins)`           |
| 翻转引脚状态   | `DL_GPIO_togglePins(gpio, pins)`          |
| 读取引脚状态   | `DL_GPIO_readPins(gpio, pins)`            |
| 写入引脚值     | `DL_GPIO_writePinsVal(gpio, pins, value)` |

### 中断相关 API

| 外设  | 启用中断                     | 禁用中断                      | 获取中断状态                      |
| ----- | ---------------------------- | ----------------------------- | --------------------------------- |
| GPIO  | `DL_GPIO_enableInterrupt()`  | `DL_GPIO_disableInterrupt()`  | `DL_GPIO_getEnabledInterrupts()`  |
| ADC12 | `DL_ADC12_enableInterrupt()` | `DL_ADC12_disableInterrupt()` | `DL_ADC12_getEnabledInterrupts()` |
| UART  | `DL_UART_enableInterrupt()`  | `DL_UART_disableInterrupt()`  | `DL_UART_getEnabledInterrupts()`  |
| I2C   | `DL_I2C_enableInterrupt()`   | `DL_I2C_disableInterrupt()`   | `DL_I2C_getEnabledInterrupts()`   |
| SPI   | `DL_SPI_enableInterrupt()`   | `DL_SPI_disableInterrupt()`   | `DL_SPI_getEnabledInterrupts()`   |
| Timer | `DL_Timer_enableInterrupt()` | `DL_Timer_disableInterrupt()` | `DL_Timer_getEnabledInterrupts()` |

### 数据传输 API

| 外设 | 发送数据（阻塞）                          | 接收数据（阻塞）                         | 发送数据（非阻塞）                | 接收数据（非阻塞）               |
| ---- | ----------------------------------------- | ---------------------------------------- | --------------------------------- | -------------------------------- |
| UART | `DL_UART_transmitDataBlocking()`          | `DL_UART_receiveDataBlocking()`          | `DL_UART_transmitData()`          | `DL_UART_receiveData()`          |
| I2C  | `DL_I2C_transmitControllerDataBlocking()` | `DL_I2C_receiveControllerDataBlocking()` | `DL_I2C_transmitControllerData()` | `DL_I2C_receiveControllerData()` |
| SPI  | `DL_SPI_transmitDataBlocking()`           | `DL_SPI_receiveDataBlocking()`           | `DL_SPI_transmitData()`           | `DL_SPI_receiveData()`           |

### 状态查询 API

| 外设  | 忙碌状态                    | FIFO 状态                                                              | 其他状态                                |
| ----- | --------------------------- | ---------------------------------------------------------------------- | --------------------------------------- |
| UART  | `DL_UART_isBusy()`          | `DL_UART_isTXFIFOEmpty()`, `DL_UART_isRXFIFOEmpty()`                   | `DL_UART_isTransmitComplete()`          |
| I2C   | `DL_I2C_isControllerBusy()` | `DL_I2C_isControllerTXFIFOEmpty()`, `DL_I2C_isControllerRXFIFOEmpty()` | `DL_I2C_isControllerTransferComplete()` |
| SPI   | `DL_SPI_isBusy()`           | `DL_SPI_isTXFIFOEmpty()`, `DL_SPI_isRXFIFOEmpty()`                     | `DL_SPI_isTransmitComplete()`           |
| ADC12 | `DL_ADC12_isBusy()`         | `DL_ADC12_isFIFOEmpty()`, `DL_ADC12_isFIFOFull()`                      | `DL_ADC12_isConversionComplete()`       |

## 外设分类

### 1. GPIO（通用输入输出）- dl_gpio.h

#### 1.1 电源管理

- **`void DL_GPIO_enablePower(GPIO_Regs* gpio)`** - 启用 GPIO 电源
  - **功能**: 启用外设写使能(PWREN)寄存器，使GPIO外设寄存器可被软件配置
  - **参数**: gpio - 指向GPIO外设寄存器的指针
  - **实现**: 通过设置PWREN寄存器的ENABLE位和适当的KEY值来启用外设
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L1835)

- **`void DL_GPIO_disablePower(GPIO_Regs* gpio)`** - 禁用 GPIO 电源
  - **功能**: 禁用外设写使能(PWREN)寄存器，使GPIO外设寄存器不可访问
  - **参数**: gpio - 指向GPIO外设寄存器的指针
  - **注意**: 此API不会提供大的功耗节省
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L1850)

- **`bool DL_GPIO_isPowerEnabled(GPIO_Regs* gpio)`** - 检查 GPIO 电源状态
  - **功能**: 检查GPIO外设写使能(PWREN)寄存器是否已启用
  - **参数**: gpio - 指向GPIO外设寄存器的指针
  - **返回值**: true - 外设寄存器访问已启用，false - 外设寄存器访问已禁用
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L1870)

#### 1.2 复位控制

- **`void DL_GPIO_reset(GPIO_Regs* gpio)`** - 复位 GPIO 模块
  - **功能**: 复位GPIO外设，清除所有配置回到默认状态
  - **参数**: gpio - 指向GPIO外设寄存器的指针
  - **实现**: 通过设置RSTCTL寄存器的RESETASSERT位和适当的KEY值来复位外设
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L1878)

- **`bool DL_GPIO_isReset(GPIO_Regs* gpio)`** - 检查 GPIO 复位状态
  - **功能**: 检查GPIO外设是否已被复位
  - **参数**: gpio - 指向GPIO外设寄存器的指针
  - **返回值**: true - 外设已被复位，false - 外设未被复位
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L1890)

#### 1.3 引脚配置

- `void DL_GPIO_initDigitalOutput(uint32_t pincmIndex)` - 初始化数字输出引脚
- `void DL_GPIO_initDigitalOutputFeatures(uint32_t pincmIndex, DL_GPIO_InternalResistor resistor, DL_GPIO_OutputStrength strength)` - 初始化数字输出引脚（带特性）
- `void DL_GPIO_initDigitalInput(uint32_t pincmIndex)` - 初始化数字输入引脚
- `void DL_GPIO_initDigitalInputFeatures(uint32_t pincmIndex, DL_GPIO_InternalResistor resistor, DL_GPIO_InputFilter filter)` - 初始化数字输入引脚（带特性）
- `void DL_GPIO_initPeripheralOutputFunction(uint32_t pincmIndex, uint32_t pincmfn)` - 初始化外设输出功能
- `void DL_GPIO_initPeripheralInputFunction(uint32_t pincmIndex, uint32_t pincmfn)` - 初始化外设输入功能
- `void DL_GPIO_initPeripheralAnalogFunction(uint32_t pincmIndex)` - 初始化外设模拟功能

#### 1.4 引脚电阻配置

- `void DL_GPIO_setDigitalInternalResistor(uint32_t pincmIndex, DL_GPIO_InternalResistor resistor)` - 设置数字内部电阻
- `void DL_GPIO_setAnalogInternalResistor(uint32_t pincmIndex, DL_GPIO_InternalResistor resistor)` - 设置模拟内部电阻

#### 1.5 引脚读写操作

- **`uint32_t DL_GPIO_readPins(GPIO_Regs* gpio, uint32_t pins)`** - 读取引脚状态
  - **功能**: 读取指定GPIO引脚的当前状态
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要读取的引脚（DL_GPIO_PIN的位或组合）
  - **返回值**: 引脚状态的32位值
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2200)

- **`void DL_GPIO_writePins(GPIO_Regs* gpio, uint32_t pins)`** - 写入引脚状态
  - **功能**: 直接写入GPIO引脚的输出值
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要写入的引脚值
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2250)

- **`void DL_GPIO_writePinsVal(GPIO_Regs* gpio, uint32_t pins, uint32_t value)`** - 写入引脚值
  - **功能**: 根据value参数设置或清除指定的GPIO引脚
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要操作的引脚，value - 要写入的值
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2230)

- **`void DL_GPIO_setPins(GPIO_Regs* gpio, uint32_t pins)`** - 设置引脚为高电平
  - **功能**: 将指定的GPIO引脚设置为高电平（1）
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要设置的引脚（DL_GPIO_PIN的位或组合）
  - **实现**: 通过写入DOUTSET寄存器来设置引脚
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2269)

- **`void DL_GPIO_clearPins(GPIO_Regs* gpio, uint32_t pins)`** - 清除引脚为低电平
  - **功能**: 将指定的GPIO引脚清除为低电平（0）
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要清除的引脚（DL_GPIO_PIN的位或组合）
  - **实现**: 通过写入DOUTCLR寄存器来清除引脚
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2279)

- **`void DL_GPIO_togglePins(GPIO_Regs* gpio, uint32_t pins)`** - 翻转引脚状态
  - **功能**: 翻转指定GPIO引脚的状态（0变1，1变0）
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要翻转的引脚（DL_GPIO_PIN的位或组合）
  - **实现**: 通过写入DOUTTGL寄存器来翻转引脚
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2289)

#### 1.6 输出控制

- **`void DL_GPIO_enableOutput(GPIO_Regs* gpio, uint32_t pins)`** - 启用引脚输出
  - **功能**: 启用指定GPIO引脚的输出功能
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要启用输出的引脚（DL_GPIO_PIN的位或组合）
  - **实现**: 通过写入DOESET寄存器来启用引脚输出
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2301)

- **`void DL_GPIO_disableOutput(GPIO_Regs* gpio, uint32_t pins)`** - 禁用引脚输出
  - **功能**: 禁用指定GPIO引脚的输出功能
  - **参数**: gpio - 指向GPIO外设寄存器的指针，pins - 要禁用输出的引脚（DL_GPIO_PIN的位或组合）
  - **实现**: 通过写入DOECLR寄存器来禁用引脚输出
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h#L2312)

#### 1.7 中断配置

- `void DL_GPIO_enableInterrupt(GPIO_Regs* gpio, uint32_t pins)` - 启用引脚中断
- `void DL_GPIO_disableInterrupt(GPIO_Regs* gpio, uint32_t pins)` - 禁用引脚中断
- `uint32_t DL_GPIO_getEnabledInterrupts(GPIO_Regs* gpio)` - 获取已启用的中断

#### 1.8 唤醒功能

- `void DL_GPIO_enableWakeUp(uint32_t pincmIndex)` - 启用引脚唤醒
- `void DL_GPIO_disableWakeUp(uint32_t pincmIndex)` - 禁用引脚唤醒
- `bool DL_GPIO_isWakeUpEnabled(uint32_t pincmIndex)` - 检查唤醒是否启用
- `void DL_GPIO_setWakeupCompareValue(uint32_t pincmIndex, DL_GPIO_WakeupCompareValue value)` - 设置唤醒比较值
- `bool DL_GPIO_isWakeStateGenerated(uint32_t pincmIndex)` - 检查是否生成唤醒状态

#### 1.9 快速唤醒

- `void DL_GPIO_enableGlobalFastWake(GPIO_Regs* gpio)` - 启用全局快速唤醒
- `void DL_GPIO_disableGlobalFastWake(GPIO_Regs* gpio)` - 禁用全局快速唤醒
- `void DL_GPIO_enableFastWakePins(GPIO_Regs* gpio, uint32_t pins)` - 启用快速唤醒引脚
- `void DL_GPIO_disableFastWakePins(GPIO_Regs* gpio, uint32_t pins)` - 禁用快速唤醒引脚
- `uint32_t DL_GPIO_getEnabledFastWakePins(GPIO_Regs* gpio)` - 获取已启用的快速唤醒引脚

#### 1.10 高阻态和滤波

- `void DL_GPIO_enableHiZ(uint32_t pincmIndex)` - 启用高阻态
- `void DL_GPIO_disableHiZ(uint32_t pincmIndex)` - 禁用高阻态
- `void DL_GPIO_setLowerPinsInputFilter(GPIO_Regs* gpio, uint32_t filter)` - 设置低位引脚输入滤波
- `void DL_GPIO_setUpperPinsInputFilter(GPIO_Regs* gpio, uint32_t filter)` - 设置高位引脚输入滤波
- `uint32_t DL_GPIO_getLowerPinsInputFilter(GPIO_Regs* gpio)` - 获取低位引脚输入滤波
- `uint32_t DL_GPIO_getUpperPinsInputFilter(GPIO_Regs* gpio)` - 获取高位引脚输入滤波

#### 1.11 引脚极性

- `void DL_GPIO_setLowerPinsPolarity(GPIO_Regs* gpio, uint32_t polarity)` - 设置低位引脚极性
- `void DL_GPIO_setUpperPinsPolarity(GPIO_Regs* gpio, uint32_t polarity)` - 设置高位引脚极性
- `uint32_t DL_GPIO_getLowerPinsPolarity(GPIO_Regs* gpio)` - 获取低位引脚极性
- `uint32_t DL_GPIO_getUpperPinsPolarity(GPIO_Regs* gpio)` - 获取高位引脚极性

#### 1.12 DMA 访问

- `void DL_GPIO_enableDMAAccess(GPIO_Regs* gpio, uint32_t pins)` - 启用 DMA 访问
- `void DL_GPIO_disableDMAAccess(GPIO_Regs* gpio, uint32_t pins)` - 禁用 DMA 访问
- `uint32_t DL_GPIO_isDMAccessEnabled(GPIO_Regs* gpio, uint32_t pins)` - 检查 DMA 访问是否启用

### 2. ADC12（12 位模数转换器）- dl_adc12.h

#### 2.1 电源管理

- **`void DL_ADC12_enablePower(ADC12_Regs *adc12)`** - 启用 ADC12 电源
  - **功能**: 启用外设写使能(PWREN)寄存器，使ADC12外设寄存器可被软件配置
  - **参数**: adc12 - 指向ADC12外设寄存器的指针
  - **实现**: 通过设置PWREN寄存器的ENABLE位和适当的KEY值来启用外设
  - **注意**: 对于功耗节省，请参考 DL_ADC12_setPowerDownMode
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_adc12.h#L1156)

- **`void DL_ADC12_disablePower(ADC12_Regs *adc12)`** - 禁用 ADC12 电源
  - **功能**: 禁用外设写使能(PWREN)寄存器，使ADC12外设寄存器不可访问
  - **参数**: adc12 - 指向ADC12外设寄存器的指针
  - **注意**: 此API不会提供大的功耗节省，对于功耗节省请参考 DL_ADC12_setPowerDownMode
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_adc12.h#L1172)

- **`bool DL_ADC12_isPowerEnabled(const ADC12_Regs *adc12)`** - 检查 ADC12 电源状态
  - **功能**: 检查ADC12外设写使能(PWREN)寄存器是否已启用
  - **参数**: adc12 - 指向ADC12外设寄存器的指针
  - **返回值**: true - 外设寄存器访问已启用，false - 外设寄存器访问已禁用
  - **源码**: [查看实现](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_adc12.h#L1189)

#### 2.2 复位控制

- `void DL_ADC12_reset(ADC12_Regs *adc12)` - 复位 ADC12 模块
- `bool DL_ADC12_isReset(const ADC12_Regs *adc12)` - 检查 ADC12 复位状态

#### 2.3 采样配置

- `void DL_ADC12_initSingleSample(ADC12_Regs *adc12, DL_ADC12_ClockSource clockSource, DL_ADC12_Resolution resolution, DL_ADC12_SamplingSource samplingSource)` - 初始化单次采样
- `void DL_ADC12_initSeqSample(ADC12_Regs *adc12, DL_ADC12_ClockSource clockSource, DL_ADC12_Resolution resolution, DL_ADC12_SamplingSource samplingSource)` - 初始化序列采样

#### 2.4 地址配置

- `void DL_ADC12_setStartAddress(ADC12_Regs *adc12, uint32_t startAdd)` - 设置开始地址
- `uint32_t DL_ADC12_getStartAddress(const ADC12_Regs *adc12)` - 获取开始地址
- `void DL_ADC12_setEndAddress(ADC12_Regs *adc12, uint32_t endAdd)` - 设置结束地址
- `uint32_t DL_ADC12_getEndAddress(const ADC12_Regs *adc12)` - 获取结束地址

#### 2.5 转换控制

- `void DL_ADC12_startConversion(ADC12_Regs *adc12)` - 开始转换
- `void DL_ADC12_stopConversion(ADC12_Regs *adc12)` - 停止转换
- `bool DL_ADC12_isConversionStarted(const ADC12_Regs *adc12)` - 检查转换是否开始

#### 2.6 使能/禁用转换

- `void DL_ADC12_enableConversions(ADC12_Regs *adc12)` - 启用转换
- `void DL_ADC12_disableConversions(ADC12_Regs *adc12)` - 禁用转换

#### 2.7 配置信息获取

- `uint32_t DL_ADC12_getResolution(const ADC12_Regs *adc12)` - 获取分辨率
- `uint32_t DL_ADC12_getDataFormat(const ADC12_Regs *adc12)` - 获取数据格式
- `uint32_t DL_ADC12_getSamplingSource(const ADC12_Regs *adc12)` - 获取采样源
- `uint32_t DL_ADC12_getSampleMode(const ADC12_Regs *adc12)` - 获取采样模式

#### 2.8 DMA 功能

- `void DL_ADC12_enableDMA(ADC12_Regs *adc12)` - 启用 DMA
- `void DL_ADC12_disableDMA(ADC12_Regs *adc12)` - 禁用 DMA
- `bool DL_ADC12_isDMAEnabled(const ADC12_Regs *adc12)` - 检查 DMA 是否启用
- `void DL_ADC12_setDMASamplesCnt(ADC12_Regs *adc12, uint32_t samples)` - 设置 DMA 采样计数

#### 2.9 FIFO 功能

- `void DL_ADC12_enableFIFO(ADC12_Regs *adc12)` - 启用 FIFO
- `void DL_ADC12_disableFIFO(ADC12_Regs *adc12)` - 禁用 FIFO
- `bool DL_ADC12_isFIFOEnabled(const ADC12_Regs *adc12)` - 检查 FIFO 是否启用

#### 2.10 时钟配置

- `void DL_ADC12_setClockConfig(ADC12_Regs *adc12, DL_ADC12_ClockConfig *config)` - 设置时钟配置
- `void DL_ADC12_getClockConfig(const ADC12_Regs *adc12, DL_ADC12_ClockConfig *config)` - 获取时钟配置

#### 2.11 功耗模式

- `void DL_ADC12_setPowerDownMode(ADC12_Regs *adc12, DL_ADC12_PowerDownMode mode)` - 设置功耗模式
- `uint32_t DL_ADC12_getPowerDownMode(const ADC12_Regs *adc12)` - 获取功耗模式

### 3. UART（通用异步收发传输器）- dl_uart.h

#### 3.1 电源管理

- `void DL_UART_enablePower(UART_Regs *uart)` - 启用 UART 电源
- `void DL_UART_disablePower(UART_Regs *uart)` - 禁用 UART 电源
- `bool DL_UART_isPowerEnabled(const UART_Regs *uart)` - 检查 UART 电源状态

#### 3.2 复位控制

- `void DL_UART_reset(UART_Regs *uart)` - 复位 UART 模块
- `bool DL_UART_isReset(const UART_Regs *uart)` - 检查 UART 复位状态

#### 3.3 使能/禁用

- `void DL_UART_enable(UART_Regs *uart)` - 启用 UART
- `void DL_UART_disable(UART_Regs *uart)` - 禁用 UART
- `bool DL_UART_isEnabled(const UART_Regs *uart)` - 检查 UART 是否启用

#### 3.4 初始化和配置

- `void DL_UART_init(UART_Regs *uart, const DL_UART_Config *config)` - 初始化 UART

#### 3.5 时钟配置

- `void DL_UART_setClockConfig(UART_Regs *uart, DL_UART_ClockConfig *config)` - 设置时钟配置
- `void DL_UART_getClockConfig(const UART_Regs *uart, DL_UART_ClockConfig *config)` - 获取时钟配置

#### 3.6 波特率配置

- `void DL_UART_configBaudRate(UART_Regs *uart, uint32_t clockFreq, uint32_t baudRate)` - 配置波特率
- `void DL_UART_setOversampling(UART_Regs *uart, DL_UART_Oversampling oversampling)` - 设置过采样

#### 3.7 工作模式

- `void DL_UART_enableLoopbackMode(UART_Regs *uart)` - 启用回环模式
- `void DL_UART_disableLoopbackMode(UART_Regs *uart)` - 禁用回环模式
- `bool DL_UART_isLoopbackModeEnabled(const UART_Regs *uart)` - 检查回环模式是否启用

#### 3.8 传输方向

- `void DL_UART_setDirection(UART_Regs *uart, DL_UART_Direction direction)` - 设置传输方向

#### 3.9 数据处理特性

- `void DL_UART_enableMajorityVoting(UART_Regs *uart)` - 启用多数投票
- `void DL_UART_disableMajorityVoting(UART_Regs *uart)` - 禁用多数投票
- `bool DL_UART_isMajorityVotingEnabled(const UART_Regs *uart)` - 检查多数投票是否启用

#### 3.10 位序配置

- `void DL_UART_enableMSBFirst(UART_Regs *uart)` - 启用 MSB 优先
- `void DL_UART_disableMSBFirst(UART_Regs *uart)` - 禁用 MSB 优先
- `bool DL_UART_isMSBFirstEnabled(const UART_Regs *uart)` - 检查 MSB 优先是否启用

#### 3.11 引脚手动控制

- `void DL_UART_enableTransmitPinManualControl(UART_Regs *uart)` - 启用发送引脚手动控制
- `void DL_UART_disableTransmitPinManualControl(UART_Regs *uart)` - 禁用发送引脚手动控制
- `bool DL_UART_isTransmitPinManualControlEnabled(const UART_Regs *uart)` - 检查发送引脚手动控制是否启用
- `void DL_UART_setTransmitPinManualOutput(UART_Regs *uart, DL_UART_TransmitPinOutput output)` - 设置发送引脚手动输出

#### 3.12 曼彻斯特编码

- `void DL_UART_enableManchesterEncoding(UART_Regs *uart)` - 启用曼彻斯特编码
- `void DL_UART_disableManchesterEncoding(UART_Regs *uart)` - 禁用曼彻斯特编码
- `bool DL_UART_isManchesterEncodingEnabled(const UART_Regs *uart)` - 检查曼彻斯特编码是否启用

#### 3.13 通信模式

- `void DL_UART_setCommunicationMode(UART_Regs *uart, DL_UART_CommunicationMode mode)` - 设置通信模式

### 4. 定时器（Timer）- dl_timer.h

#### 4.1 电源管理

- `void DL_Timer_enablePower(GPTIMER_Regs *gptimer)` - 启用定时器电源
- `void DL_Timer_disablePower(GPTIMER_Regs *gptimer)` - 禁用定时器电源
- `bool DL_Timer_isPowerEnabled(const GPTIMER_Regs *gptimer)` - 检查定时器电源状态

#### 4.2 复位控制

- `void DL_Timer_reset(GPTIMER_Regs *gptimer)` - 复位定时器
- `bool DL_Timer_isReset(const GPTIMER_Regs *gptimer)` - 检查定时器复位状态

#### 4.3 时钟配置

- `void DL_Timer_setClockConfig(GPTIMER_Regs *gptimer, DL_Timer_ClockConfig *config)` - 设置时钟配置
- `void DL_Timer_getClockConfig(const GPTIMER_Regs *gptimer, DL_Timer_ClockConfig *config)` - 获取时钟配置
- `void DL_Timer_enableClock(GPTIMER_Regs *gptimer)` - 启用定时器时钟
- `void DL_Timer_disableClock(GPTIMER_Regs *gptimer)` - 禁用定时器时钟
- `bool DL_Timer_isClockEnabled(const GPTIMER_Regs *gptimer)` - 检查时钟是否启用

#### 4.4 定时器值操作

- `void DL_Timer_setLoadValue(GPTIMER_Regs *gptimer, uint32_t loadValue)` - 设置加载值
- `uint32_t DL_Timer_getLoadValue(const GPTIMER_Regs *gptimer)` - 获取加载值
- `uint32_t DL_Timer_getTimerCount(const GPTIMER_Regs *gptimer)` - 获取定时器计数值
- `void DL_Timer_setTimerCount(GPTIMER_Regs *gptimer, uint32_t count)` - 设置定时器计数值

#### 4.5 捕获/比较配置

- `void DL_Timer_setCCPDirection(GPTIMER_Regs *gptimer, DL_Timer_CCPDirection direction)` - 设置捕获/比较方向
- `uint32_t DL_Timer_getCCPDirection(const GPTIMER_Regs *gptimer)` - 获取捕获/比较方向
- `void DL_Timer_setCCPOutputDisabled(GPTIMER_Regs *gptimer, DL_Timer_CCIndex ccIndex)` - 设置捕获/比较输出禁用
- `void DL_Timer_setCCPOutputDisabledAdv(GPTIMER_Regs *gptimer, DL_Timer_CCIndex ccIndex, DL_Timer_CCPOutputDisabled disabled)` - 设置捕获/比较输出禁用（高级）

#### 4.6 交叉触发配置

- `void DL_Timer_configCrossTrigger(GPTIMER_Regs *gptimer, DL_Timer_CrossTriggerConfig *config)` - 配置交叉触发
- `void DL_Timer_configCrossTriggerSrc(GPTIMER_Regs *gptimer, DL_Timer_CrossTriggerSrc src)` - 配置交叉触发源
- `void DL_Timer_configCrossTriggerInputCond(GPTIMER_Regs *gptimer, DL_Timer_CrossTriggerInputCond cond)` - 配置交叉触发输入条件
- `void DL_Timer_configCrossTriggerEnable(GPTIMER_Regs *gptimer, bool enable)` - 配置交叉触发启用
- `uint32_t DL_Timer_getCrossTriggerConfig(const GPTIMER_Regs *gptimer)` - 获取交叉触发配置
- `void DL_Timer_generateCrossTrigger(GPTIMER_Regs *gptimer)` - 生成交叉触发

#### 4.7 影子特性

- `void DL_Timer_enableShadowFeatures(GPTIMER_Regs *gptimer)` - 启用影子特性
- `void DL_Timer_disableShadowFeatures(GPTIMER_Regs *gptimer)` - 禁用影子特性

#### 4.8 定时器 A 相关宏定义

- `DL_TIMERA_CAPTURE_COMPARE_0_INDEX` - 捕获/比较通道 0 索引
- `DL_TIMERA_CAPTURE_COMPARE_1_INDEX` - 捕获/比较通道 1 索引
- `DL_TIMERA_CAPTURE_COMPARE_2_INDEX` - 捕获/比较通道 2 索引
- `DL_TIMERA_CAPTURE_COMPARE_3_INDEX` - 捕获/比较通道 3 索引

#### 4.9 中断事件

- `DL_TIMERA_INTERRUPT_ZERO_EVENT` - 零事件中断
- `DL_TIMERA_INTERRUPT_LOAD_EVENT` - 加载事件中断
- `DL_TIMERA_INTERRUPT_CC0_DN_EVENT` - 捕获/比较 0 下降沿事件中断
- `DL_TIMERA_INTERRUPT_CC1_DN_EVENT` - 捕获/比较 1 下降沿事件中断
- `DL_TIMERA_INTERRUPT_CC0_UP_EVENT` - 捕获/比较 0 上升沿事件中断
- `DL_TIMERA_INTERRUPT_CC1_UP_EVENT` - 捕获/比较 1 上升沿事件中断
- `DL_TIMERA_INTERRUPT_OVERFLOW_EVENT` - 溢出事件中断
- `DL_TIMERA_INTERRUPT_CC2_DN_EVENT` - 捕获/比较 2 下降沿事件中断
- `DL_TIMERA_INTERRUPT_CC3_DN_EVENT` - 捕获/比较 3 下降沿事件中断
- `DL_TIMERA_INTERRUPT_CC2_UP_EVENT` - 捕获/比较 2 上升沿事件中断
- `DL_TIMERA_INTERRUPT_CC3_UP_EVENT` - 捕获/比较 3 上升沿事件中断

#### 4.10 配置保存和恢复

- `bool DL_TimerA_saveConfiguration(GPTIMER_Regs *gptimer, DL_TimerA_backupConfig *ptr)` - 保存定时器 A 配置
- `bool DL_TimerA_restoreConfiguration(GPTIMER_Regs *gptimer, DL_TimerA_backupConfig *ptr)` - 恢复定时器 A 配置

### 5. 其他外设模块

#### 5.1 AES 加密模块 - dl_aes.h / dl_aesadv.h

- 高级加密标准（AES）硬件加速
- 支持 128/192/256 位密钥长度
- 支持 ECB、CBC、CFB、OFB、CTR 等多种工作模式

#### 5.2 比较器模块 - dl_comp.h

- 模拟比较器功能
- 支持内部参考电压
- 支持迟滞功能

#### 5.3 CRC 模块 - dl_crc.h / dl_crcp.h

- 循环冗余校验硬件加速
- 支持多种 CRC 多项式
- 支持 DMA 传输

#### 5.4 DAC12 模块 - dl_dac12.h

- 12 位数模转换器
- 支持多种输出模式
- 支持 DMA 传输

#### 5.5 DMA 模块 - dl_dma.h

##### 5.5.1 通道初始化和配置

- `void DL_DMA_initChannel(DMA_Regs *dma, uint8_t channelNum, DL_DMA_Config *config)` - 初始化 DMA 通道
- `void DL_DMA_configTransfer(DMA_Regs *dma, uint8_t channelNum, void *srcAddr, void *destAddr, uint32_t size)` - 配置传输

##### 5.5.2 优先级控制

- `void DL_DMA_enableRoundRobinPriority(DMA_Regs *dma)` - 启用轮询优先级
- `void DL_DMA_disableRoundRobinPriority(DMA_Regs *dma)` - 禁用轮询优先级
- `bool DL_DMA_isRoundRobinPriorityEnabled(const DMA_Regs *dma)` - 检查轮询优先级是否启用

##### 5.5.3 通道控制

- `void DL_DMA_enableChannel(DMA_Regs *dma, uint8_t channelNum)` - 启用 DMA 通道
- `void DL_DMA_disableChannel(DMA_Regs *dma, uint8_t channelNum)` - 禁用 DMA 通道
- `bool DL_DMA_isChannelEnabled(const DMA_Regs *dma, uint8_t channelNum)` - 检查通道是否启用

##### 5.5.4 传输控制

- `void DL_DMA_startTransfer(DMA_Regs *dma, uint8_t channelNum)` - 开始传输
- `void DL_DMA_setBurstSize(DMA_Regs *dma, uint8_t channelNum, DL_DMA_BurstSize burstSize)` - 设置突发大小
- `void DL_DMA_setTransferMode(DMA_Regs *dma, uint8_t channelNum, DL_DMA_TransferMode mode)` - 设置传输模式
- `void DL_DMA_setExtendedMode(DMA_Regs *dma, uint8_t channelNum, DL_DMA_ExtendedMode mode)` - 设置扩展模式

##### 5.5.5 触发器配置

- `void DL_DMA_setTrigger(DMA_Regs *dma, uint8_t channelNum, DL_DMA_Trigger trigger)` - 设置触发器
- `uint32_t DL_DMA_getTrigger(const DMA_Regs *dma, uint8_t channelNum)` - 获取触发器

##### 5.5.6 地址配置

- `void DL_DMA_setSrcAddr(DMA_Regs *dma, uint8_t channelNum, void *srcAddr)` - 设置源地址
- `uint32_t DL_DMA_getSrcAddr(const DMA_Regs *dma, uint8_t channelNum)` - 获取源地址
- `void DL_DMA_setDestAddr(DMA_Regs *dma, uint8_t channelNum, void *destAddr)` - 设置目标地址
- `uint32_t DL_DMA_getDestAddr(const DMA_Regs *dma, uint8_t channelNum)` - 获取目标地址

##### 5.5.7 传输大小和增量

- `void DL_DMA_setTransferSize(DMA_Regs *dma, uint8_t channelNum, uint32_t size)` - 设置传输大小
- `void DL_DMA_setSrcIncrement(DMA_Regs *dma, uint8_t channelNum, DL_DMA_Increment increment)` - 设置源地址增量
- `void DL_DMA_setDestIncrement(DMA_Regs *dma, uint8_t channelNum, DL_DMA_Increment increment)` - 设置目标地址增量

##### 5.5.8 数据宽度

- `void DL_DMA_setSrcWidth(DMA_Regs *dma, uint8_t channelNum, DL_DMA_Width width)` - 设置源数据宽度
- `void DL_DMA_setDestWidth(DMA_Regs *dma, uint8_t channelNum, DL_DMA_Width width)` - 设置目标数据宽度

##### 5.5.9 中断配置

- `void DL_DMA_enableInterrupt(DMA_Regs *dma, uint8_t channelNum, uint32_t interruptMask)` - 启用中断
- `void DL_DMA_Full_Ch_setEarlyInterruptThreshold(DMA_Regs *dma, uint8_t channelNum, uint32_t threshold)` - 设置早期中断阈值

#### 5.6 Flash 控制器 - dl_flashctl.h

- Flash 存储器控制
- 支持编程和擦除操作
- 支持读保护

#### 5.7 运算放大器 - dl_gpamp.h / dl_opa.h

- 通用运算放大器
- 支持多种增益配置
- 支持缓冲模式

#### 5.8 I2C 模块 - dl_i2c.h

##### 5.8.1 电源管理

- `void DL_I2C_enablePower(I2C_Regs *i2c)` - 启用 I2C 电源
- `void DL_I2C_disablePower(I2C_Regs *i2c)` - 禁用 I2C 电源
- `bool DL_I2C_isPowerEnabled(const I2C_Regs *i2c)` - 检查 I2C 电源状态

##### 5.8.2 时钟配置

- `void DL_I2C_setClockConfig(I2C_Regs *i2c, const DL_I2C_ClockConfig *config)` - 设置时钟配置
- `void DL_I2C_getClockConfig(const I2C_Regs *i2c, DL_I2C_ClockConfig *config)` - 获取时钟配置

##### 5.8.3 控制器模式（主机）

- `void DL_I2C_flushControllerTXFIFO(I2C_Regs *i2c)` - 清空控制器发送 FIFO
- `void DL_I2C_flushControllerRXFIFO(I2C_Regs *i2c)` - 清空控制器接收 FIFO
- `bool DL_I2C_isControllerTXFIFOFull(const I2C_Regs *i2c)` - 检查控制器发送 FIFO 是否满
- `bool DL_I2C_isControllerTXFIFOEmpty(const I2C_Regs *i2c)` - 检查控制器发送 FIFO 是否为空
- `bool DL_I2C_isControllerRXFIFOEmpty(const I2C_Regs *i2c)` - 检查控制器接收 FIFO 是否为空
- `void DL_I2C_resetControllerTransfer(I2C_Regs *i2c)` - 复位控制器传输
- `void DL_I2C_startControllerTransfer(I2C_Regs *i2c, uint32_t targetAddress, DL_I2C_CMD_TYPE cmdType, uint32_t length)` - 启动控制器传输
- `void DL_I2C_startControllerTransferAdvanced(I2C_Regs *i2c, uint32_t targetAddress, DL_I2C_CMD_TYPE cmdType, uint32_t length, bool restart)` - 启动控制器高级传输

##### 5.8.4 目标模式（从机）

- `bool DL_I2C_isTargetTXFIFOFull(const I2C_Regs *i2c)` - 检查目标发送 FIFO 是否满
- `bool DL_I2C_isTargetTXFIFOEmpty(const I2C_Regs *i2c)` - 检查目标发送 FIFO 是否为空
- `bool DL_I2C_isTargetRXFIFOEmpty(const I2C_Regs *i2c)` - 检查目标接收 FIFO 是否为空
- `void DL_I2C_flushTargetTXFIFO(I2C_Regs *i2c)` - 清空目标发送 FIFO
- `void DL_I2C_flushTargetRXFIFO(I2C_Regs *i2c)` - 清空目标接收 FIFO
- `void DL_I2C_transmitTargetDataBlocking(I2C_Regs *i2c, uint8_t data)` - 目标阻塞发送数据
- `bool DL_I2C_transmitTargetDataCheck(I2C_Regs *i2c, uint8_t data)` - 目标非阻塞发送数据
- `bool DL_I2C_receiveTargetDataCheck(const I2C_Regs *i2c, uint8_t *buffer)` - 目标非阻塞接收数据

#### 5.9 独立看门狗 - dl_iwdt.h

- 独立看门狗定时器
- 支持多种超时周期
- 支持窗口模式

#### 5.10 密钥存储控制器 - dl_keystorectl.h

- 安全密钥存储
- 支持密钥生成和管理
- 支持硬件加密

#### 5.11 LCD 控制器 - dl_lcd.h

- 液晶显示控制器
- 支持多种显示模式
- 支持低功耗操作

#### 5.12 低频子系统 - dl_lfss.h

- 低频时钟和计时功能
- 支持 RTC 功能
- 支持低功耗模式

#### 5.13 数学加速器 - dl_mathacl.h

- 硬件数学运算加速
- 支持三角函数计算
- 支持对数和指数运算

#### 5.14 CAN 控制器 - dl_mcan.h

- 支持 CAN FD 协议
- 支持消息过滤
- 支持错误处理

#### 5.15 RTC 模块 - dl_rtc.h / dl_rtc_a.h / dl_rtc_b.h

- 实时时钟功能
- 支持日历功能
- 支持闹钟和定时器

#### 5.16 SPI 模块 - dl_spi.h

##### 5.16.1 电源管理

- `void DL_SPI_enablePower(SPI_Regs *spi)` - 启用 SPI 电源
- `void DL_SPI_disablePower(SPI_Regs *spi)` - 禁用 SPI 电源
- `bool DL_SPI_isPowerEnabled(const SPI_Regs *spi)` - 检查 SPI 电源状态

##### 5.16.2 复位和使能控制

- `void DL_SPI_reset(SPI_Regs *spi)` - 复位 SPI 模块
- `bool DL_SPI_isReset(const SPI_Regs *spi)` - 检查 SPI 复位状态
- `void DL_SPI_enable(SPI_Regs *spi)` - 启用 SPI
- `void DL_SPI_disable(SPI_Regs *spi)` - 禁用 SPI
- `bool DL_SPI_isEnabled(const SPI_Regs *spi)` - 检查 SPI 是否启用

##### 5.16.3 初始化和配置

- `void DL_SPI_init(SPI_Regs *spi, const DL_SPI_Config *config)` - 初始化 SPI
- `void DL_SPI_setClockConfig(SPI_Regs *spi, const DL_SPI_ClockConfig *config)` - 设置时钟配置
- `void DL_SPI_getClockConfig(const SPI_Regs *spi, DL_SPI_ClockConfig *config)` - 获取时钟配置

##### 5.16.4 状态查询

- `bool DL_SPI_isBusy(const SPI_Regs *spi)` - 检查 SPI 是否忙碌
- `bool DL_SPI_isTXFIFOEmpty(const SPI_Regs *spi)` - 检查发送 FIFO 是否为空
- `bool DL_SPI_isTXFIFOFull(const SPI_Regs *spi)` - 检查发送 FIFO 是否满
- `bool DL_SPI_isRXFIFOEmpty(const SPI_Regs *spi)` - 检查接收 FIFO 是否为空
- `bool DL_SPI_isRXFIFOFull(const SPI_Regs *spi)` - 检查接收 FIFO 是否满

##### 5.16.5 奇偶校验

- `void DL_SPI_setParity(SPI_Regs *spi, DL_SPI_PARITY parity)` - 设置奇偶校验
- `void DL_SPI_enableReceiveParity(SPI_Regs *spi)` - 启用接收奇偶校验
- `void DL_SPI_disableReceiveParity(SPI_Regs *spi)` - 禁用接收奇偶校验
- `bool DL_SPI_isReceiveParityEnabled(const SPI_Regs *spi)` - 检查接收奇偶校验是否启用
- `void DL_SPI_enableTransmitParity(SPI_Regs *spi)` - 启用发送奇偶校验

#### 5.17 防篡改 I/O - dl_tamperio.h

- 防篡改检测
- 支持多种触发条件
- 支持安全擦除

#### 5.18 真随机数生成器 - dl_trng.h

- 硬件随机数生成
- 支持多种熵源
- 符合安全标准

#### 5.19 基准电压 - dl_vref.h

- 内部基准电压源
- 支持多种电压等级
- 支持温度补偿

#### 5.20 窗口看门狗 - dl_wwdt.h

- 窗口看门狗定时器
- 支持窗口模式
- 支持多种超时周期

### 6. 系统级模块

#### 6.1 系统控制 - m0p/dl_sysctl.h

- 系统时钟控制
- 电源管理
- 复位控制

#### 6.2 中断控制 - m0p/dl_interrupt.h

- 中断使能/禁用
- 中断优先级设置
- 中断向量表管理

#### 6.3 SysTick 定时器 - m0p/dl_systick.h

- 系统定时器
- 支持多种计数模式
- 支持中断生成

#### 6.4 工厂区域 - m0p/dl_factoryregion.h

- 工厂预设参数
- 校准数据访问
- 设备信息读取

### 7. 通用定义 - dl_common.h

- 通用数据类型定义
- 错误码定义
- 通用宏定义

## 使用说明

### 头文件包含

```c
#include <ti/driverlib/driverlib.h>
```

### 基本使用流程

1. 使能外设电源：`DL_xxx_enablePower()`
2. 配置外设参数：`DL_xxx_init()` 或相关配置函数
3. 使能外设：`DL_xxx_enable()`
4. 执行操作：根据具体外设调用相应 API
5. 处理中断：在中断服务程序中调用相关 API

### 常用代码示例

#### GPIO 控制示例

```c
// 初始化GPIO输出
DL_GPIO_initDigitalOutput(IOMUX_PINCM22);

// 设置GPIO为高电平
DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_0);

// 设置GPIO为低电平
DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_0);

// 翻转GPIO状态
DL_GPIO_togglePins(GPIOA, DL_GPIO_PIN_0);

// 读取GPIO状态
uint32_t pinState = DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_0);
```

#### ADC 使用示例

```c
// 启用ADC电源
DL_ADC12_enablePower(ADC12_0_INST);

// 初始化ADC单次采样
DL_ADC12_initSingleSample(ADC12_0_INST,
                         DL_ADC12_CLOCK_ULPCLK,
                         DL_ADC12_RESOLUTION_12_BIT,
                         DL_ADC12_SAMPLING_SOURCE_AUTO);

// 启用ADC转换
DL_ADC12_enableConversions(ADC12_0_INST);

// 开始转换
DL_ADC12_startConversion(ADC12_0_INST);

// 等待转换完成并读取结果
while (!DL_ADC12_isConversionComplete(ADC12_0_INST));
uint16_t adcResult = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
```

#### UART 通信示例

```c
// UART配置结构体
DL_UART_Config uartConfig = {
    .mode = DL_UART_MODE_NORMAL,
    .direction = DL_UART_DIRECTION_TX_RX,
    .flowControl = DL_UART_FLOW_CONTROL_NONE,
    .parity = DL_UART_PARITY_NONE,
    .wordLength = DL_UART_WORD_LENGTH_8_BITS,
    .stopBits = DL_UART_STOP_BITS_ONE
};

// 启用UART电源
DL_UART_enablePower(UART0_INST);

// 初始化UART
DL_UART_init(UART0_INST, &uartConfig);

// 配置波特率
DL_UART_configBaudRate(UART0_INST, 32000000, 115200);

// 启用UART
DL_UART_enable(UART0_INST);

// 发送数据
DL_UART_transmitDataBlocking(UART0_INST, 'A');

// 接收数据
char receivedData = DL_UART_receiveDataBlocking(UART0_INST);
```

#### 定时器使用示例

```c
// 启用定时器电源
DL_Timer_enablePower(TIMG0_INST);

// 设置定时器加载值（1秒定时，假设时钟32MHz）
DL_Timer_setLoadValue(TIMG0_INST, 32000000);

// 启用定时器时钟
DL_Timer_enableClock(TIMG0_INST);

// 启用定时器中断
DL_Timer_enableInterrupt(TIMG0_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);

// 启动定时器
DL_Timer_startCounter(TIMG0_INST);
```

#### I2C 主机模式示例

```c
// 启用I2C电源
DL_I2C_enablePower(I2C0_INST);

// 复位I2C
DL_I2C_reset(I2C0_INST);

// 配置I2C为控制器模式
DL_I2C_setControllerMode(I2C0_INST, DL_I2C_CONTROLLER_MODE_STANDARD);

// 启用I2C
DL_I2C_enable(I2C0_INST);

// 发送数据
uint8_t txData = 0x55;
DL_I2C_startControllerTransfer(I2C0_INST, 0x48, DL_I2C_CONTROLLER_DIRECTION_TX, 1);
DL_I2C_transmitControllerData(I2C0_INST, txData);

// 接收数据
uint8_t rxData;
DL_I2C_startControllerTransfer(I2C0_INST, 0x48, DL_I2C_CONTROLLER_DIRECTION_RX, 1);
rxData = DL_I2C_receiveControllerData(I2C0_INST);
```

#### SPI 主机模式示例

```c
// SPI配置结构体
DL_SPI_Config spiConfig = {
    .mode = DL_SPI_MODE_CONTROLLER,
    .frameFormat = DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0,
    .dataSize = DL_SPI_DATA_SIZE_8,
    .bitOrder = DL_SPI_BIT_ORDER_MSB_FIRST,
    .chipSelectPin = DL_SPI_CHIP_SELECT_0
};

// 启用SPI电源
DL_SPI_enablePower(SPI0_INST);

// 初始化SPI
DL_SPI_init(SPI0_INST, &spiConfig);

// 启用SPI
DL_SPI_enable(SPI0_INST);

// 发送数据
DL_SPI_transmitDataBlocking(SPI0_INST, 0xAA);

// 接收数据
uint8_t receivedData = DL_SPI_receiveDataBlocking(SPI0_INST);
```

### 注意事项

- 所有 API 函数都来自 TI 官方 SDK 2.05.00.05 版本
- 使用前请确保对应外设的电源已启用
- 中断使用需要配置相应的中断向量
- 部分 API 需要先复位外设再进行配置
- 在使用 DMA 时，确保内存地址对齐
- 使用浮点运算时需要启用 FPU（如果芯片支持）
- 低功耗模式下，某些外设可能无法正常工作

### 错误处理

- 大多数 API 函数返回布尔值表示成功/失败
- 检查返回值以确保操作成功
- 使用相应的状态查询函数验证外设状态
- 在中断服务程序中及时清除中断标志

### 性能优化建议

- 使用 DMA 进行大量数据传输
- 在不需要时禁用外设以降低功耗
- 使用硬件加速器进行数学运算
- 合理配置时钟频率以平衡性能和功耗

## 版本信息

- SDK 版本：2.05.00.05
- 支持器件：MSPM0G3507 系列
- 文档生成时间：2025 年 7 月

## API 源码导航索引

### 核心驱动库文件位置

所有驱动库头文件位于：`mspm0_sdk_2_05_00_05/source/ti/driverlib/`

| 外设模块 | 头文件名 | 源码路径 |
|----------|----------|----------|
| GPIO | dl_gpio.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_gpio.h) |
| ADC12 | dl_adc12.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_adc12.h) |
| UART | dl_uart.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_uart.h) |
| I2C | dl_i2c.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_i2c.h) |
| SPI | dl_spi.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_spi.h) |
| Timer | dl_timer.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_timer.h) |
| TimerA | dl_timera.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_timera.h) |
| TimerG | dl_timerg.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_timerg.h) |
| DMA | dl_dma.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_dma.h) |
| Flash | dl_flashctl.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_flashctl.h) |
| Common | dl_common.h | [查看源码](../mspm0_sdk_2_05_00_05/source/ti/driverlib/dl_common.h) |

### 实现细节说明

#### 函数命名规范

TI MSPM0 驱动库遵循统一的命名规范：

- **前缀**: 所有函数以 `DL_` 开头
- **模块名**: 紧随其后的是模块名，如 `GPIO`, `ADC12`, `UART` 等
- **功能描述**: 最后是具体的功能描述

#### 内联函数实现

大多数驱动库函数都是内联函数(`__STATIC_INLINE`)，这意味着：

1. **性能优化**: 函数调用开销最小化
2. **代码优化**: 编译器可以进行更好的优化
3. **直接寄存器访问**: 直接操作硬件寄存器

#### 寄存器访问模式

驱动库采用结构化寄存器访问方式：

```c
// 例如GPIO设置引脚的实现
__STATIC_INLINE void DL_GPIO_setPins(GPIO_Regs* gpio, uint32_t pins)
{
    gpio->DOUTSET31_0 = pins;  // 直接写入寄存器
}
```

#### 电源管理模式

所有外设都遵循统一的电源管理模式：

1. **启用电源** (`enablePower`) - 启用外设时钟和电源
2. **复位外设** (`reset`) - 将外设复位到默认状态
3. **配置外设** (各种配置函数) - 设置外设参数
4. **启用外设** (`enable`) - 启用外设功能

#### 错误处理

驱动库函数通常不包含错误检查代码，以保证最佳性能。开发者需要：

1. **参数验证**: 确保传入的参数有效
2. **状态检查**: 在操作前检查外设状态
3. **中断处理**: 正确处理中断和错误状态

#### 典型使用流程

```c
// 标准外设初始化流程
DL_XXX_enablePower(PERIPHERAL_INST);    // 1. 启用电源
DL_XXX_reset(PERIPHERAL_INST);          // 2. 复位外设
// 3. 配置外设参数
DL_XXX_configureXXX(PERIPHERAL_INST, config);
DL_XXX_enable(PERIPHERAL_INST);         // 4. 启用外设
```

### 调试技巧

1. **寄存器查看**: 使用调试器查看寄存器值来验证配置
2. **中断向量**: 确保正确配置中断向量表
3. **时钟配置**: 验证系统时钟和外设时钟配置正确

### 性能优化建议

1. **批量操作**: 使用位操作进行批量GPIO操作
2. **DMA使用**: 对于大量数据传输，优先使用DMA
3. **中断优先级**: 合理设置中断优先级
4. **低功耗模式**: 合理使用低功耗模式节省电力

## 文档更新记录

- **v1.0** (2025-07-13): 初始版本，包含基本API列表
- **v1.1** (2025-07-13): 添加详细的函数说明、源码链接和实现细节
  - 为每个API函数添加了详细的功能描述
  - 添加了源码文件的相对路径链接
  - 根据头文件注释添加了参数和返回值说明
  - 添加了API源码导航索引和实现细节说明

# AD9954 引脚配置对比表

## 当前配置（更新后）

| 功能     | STM32 引脚 | CubeMX User Label | 引脚类型 |
| -------- | ---------- | ----------------- | -------- |
| PS1      | PA12       | PS1               | 输出     |
| SPI CS   | PA11       | S_CS              | 输出     |
| SPI SCLK | PA10       | S_SCLK            | 输出     |
| SPI SDIO | PA9        | S_DIO             | 输出     |
| 复位     | PA8        | AD9954_RES        | 输出     |
| 更新     | PC9        | IOUPDATE          | 输出     |
| 电源     | PC8        | AD9954_PWR        | 输出     |
| 同步     | PC7        | AD9954_IOSY       | 输出     |
| PS0      | PC6        | PS0               | 输出     |
| OSK      | PD15       | AD9954_OSK        | 输出     |
| SPI SDO  | PD14       | S_SDO             | 输入     |

## 原始配置（重构前）

| 功能     | STM32 引脚 | 引脚类型 |
| -------- | ---------- | -------- |
| PS1      | PA2        | 输出     |
| SPI CS   | PA3        | 输出     |
| SPI SCLK | PA4        | 输出     |
| SPI SDIO | PA5        | 输出     |
| 复位     | PA6        | 输出     |
| 更新     | PA7        | 输出     |
| SPI SDO  | PA8        | 输入     |
| 电源     | PB0        | 输出     |
| 同步     | PB1        | 输出     |
| PS0      | PB10       | 输出     |
| OSK      | PC0        | 输出     |

## 配置说明

1. **输出引脚配置**：

   - 模式：Push-Pull Output
   - 速度：GPIO_SPEED_FREQ_HIGH
   - 上拉：无需设置

2. **输入引脚配置**：

   - 模式：Input Pull-Up
   - 上拉：启用

3. **在 CubeMX 中配置时**：
   - 每个引脚都要设置对应的 User Label
   - 确保所有引脚都已正确配置
   - 生成代码后检查 GPIO 初始化函数

## 使用注意事项

- 请确保您的硬件连接与新的引脚配置一致
- 如果硬件无法更改，请修改头文件中的引脚定义
- 所有引脚都需要在 CubeMX 中正确配置后才能正常工作

#ifndef __AD9554_H
#define __AD9554_H

#include "stm32h7xx_hal.h"
#include "main.h"

/*==============================================================================
 * 重要说明：GPIO初始化要求
 *============================================================================*/
/*
 * 本驱动库不再包含GPIO初始化代码，用户需要：
 * 1. 在CubeMX中配置下面定义的GPIO引脚
 * 2. 或者手动初始化这些GPIO引脚
 * 3. 确保在调用AD9554_Init()之前已完成GPIO初始化
 * 
 * 详细配置方法请参考：CUBEMX_CONFIG_GUIDE.md
 */

/*==============================================================================
 * AD9554 软件SPI驱动配置
 *============================================================================*/

/* 软件SPI配置 */
#define AD9554_USE_SOFT_SPI         1    // 1: 软件SPI, 0: 硬件SPI

/*==============================================================================
 * AD9554 软件SPI引脚配置
 *============================================================================*/
// 引脚定义说明:
// CS (Chip Select)     - 片选信号，低电平有效
// SCLK (Serial Clock)  - 串行时钟，上升沿有效
// SDIO (Serial Data I/O) - 串行数据输入/输出
// SDO (Serial Data Out) - 串行数据输出 (从AD9554读取)
// OSK (Output Shift Key) - 输出移位键控
// PS0/PS1 (Profile Select) - 配置文件选择
// IOUPDATE - I/O更新信号
// RES (Reset) - 复位信号，低电平有效
// PWR (Power Down) - 功率控制

// 引脚连接配置 (可根据实际硬件连接修改)
#define AD9554_SPI_CS_PORT     GPIOF
#define AD9554_SPI_CS_PIN      GPIO_PIN_12
#define AD9554_SPI_SCLK_PORT   GPIOF
#define AD9554_SPI_SCLK_PIN    GPIO_PIN_14
#define AD9554_SPI_SDIO_PORT   GPIOG
#define AD9554_SPI_SDIO_PIN    GPIO_PIN_0
#define AD9554_SPI_SDO_PORT    GPIOF
#define AD9554_SPI_SDO_PIN     GPIO_PIN_11

#define AD9554_OSK_PORT        GPIOE
#define AD9554_OSK_PIN         GPIO_PIN_7
#define AD9554_PS0_PORT        GPIOE
#define AD9554_PS0_PIN         GPIO_PIN_9
#define AD9554_PS1_PORT        GPIOE
#define AD9554_PS1_PIN         GPIO_PIN_11
#define AD9554_IOUPDATE_PORT   GPIOE
#define AD9554_IOUPDATE_PIN    GPIO_PIN_13
#define AD9554_RES_PORT        GPIOF
#define AD9554_RES_PIN         GPIO_PIN_15
#define AD9554_PWR_PORT        GPIOG
#define AD9554_PWR_PIN         GPIO_PIN_1

/*==============================================================================
 * AD9554 寄存器地址定义
 *============================================================================*/
#define AD9554_REG_CFR1           0x00    // 控制功能寄存器1
#define AD9554_REG_CFR2           0x01    // 控制功能寄存器2
#define AD9554_REG_ASF            0x02    // 幅度比例因子
#define AD9554_REG_ARR            0x03    // 幅度斜率率
#define AD9554_REG_FTW0           0x04    // 频率调节字0
#define AD9554_REG_POW0           0x05    // 相位偏移字0
#define AD9554_REG_FTW1           0x06    // 频率调节字1
#define AD9554_REG_NLSCW          0x07    // 负线性扫描控制字
#define AD9554_REG_PLSCW          0x08    // 正线性扫描控制字
#define AD9554_REG_RSCW0          0x07    // 斜率扫描控制字0
#define AD9554_REG_RSCW1          0x08    // 斜率扫描控制字1
#define AD9554_REG_RSCW2          0x09    // 斜率扫描控制字2
#define AD9554_REG_RSCW3          0x0A    // 斜率扫描控制字3
#define AD9554_REG_RAM            0x0B    // RAM

// 控制位定义
#define AD9554_NO_DWELL           0x80    // 无驻留控制位

/*==============================================================================
 * AD9554 配置参数
 *============================================================================*/
#define AD9554_SYSCLK_MHZ         400     // 系统时钟频率 (MHz)
#define AD9554_PLL_MULTIPLIER     20      // PLL倍频系数

// 频率转换计算参数
#define AD9554_FTW_MULTIPLIER     10.7374 // 频率调节字转换系数

/*==============================================================================
 * AD9554 扫描模式枚举
 *============================================================================*/
typedef enum {
    AD9554_SCAN_DOWN = 0,     // 下扫描
    AD9554_SCAN_UP,           // 上扫描
    AD9554_SCAN_DOUBLE        // 双向扫描
} AD9554_ScanMode_t;

/*==============================================================================
 * AD9554 函数声明
 *============================================================================*/
// 初始化函数
void AD9554_Init(void);

// 软件SPI底层函数
void AD9554_SPI_WriteByte(uint8_t data);
uint8_t AD9554_SPI_ReadByte(void);

// 控制函数
void AD9554_Reset(void);
void AD9554_IOUpdate(void);

// 配置函数
void AD9554_SetFrequency(double frequency_hz);
void AD9554_SetAmplitude(uint16_t amplitude);
void AD9554_SetPhase(uint16_t phase);

// 扫描模式函数
void AD9554_SetScanMode(AD9554_ScanMode_t mode);

// 工具函数
uint32_t AD9554_FrequencyToFTW(double frequency_hz);
double AD9554_FTWToFrequency(uint32_t ftw);

// 延时函数声明 (需要用户实现)
void AD9554_DelayMs(uint32_t ms);
void AD9554_DelayUs(uint32_t us);

#endif /* __AD9554_H */

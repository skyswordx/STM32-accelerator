#ifndef AD5933_HAL_H
#define AD5933_HAL_H

#include "stm32h7xx_hal.h"  // 根据您的STM32型号调整，如stm32f4xx_hal.h等
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#define AD5933_FREQ 16384000UL
#define PI 3.14159265358979323846f

// AD5933 I2C 地址
#define AD5933_ADDR             (0x1A << 1)  // 7位地址左移1位
#define AD5933_WRITE_ADDR       AD5933_ADDR
#define AD5933_READ_ADDR        (AD5933_ADDR | 0x01)

// AD5933 寄存器地址
#define AD5933_REG_CONTROL_HIGH     0x80
#define AD5933_REG_CONTROL_LOW      0x81
#define AD5933_REG_START_FREQ_HIGH  0x82
#define AD5933_REG_START_FREQ_MID   0x83
#define AD5933_REG_START_FREQ_LOW   0x84
#define AD5933_REG_FREQ_INC_HIGH    0x85
#define AD5933_REG_FREQ_INC_MID     0x86
#define AD5933_REG_FREQ_INC_LOW     0x87
#define AD5933_REG_NUM_INC_HIGH     0x88
#define AD5933_REG_NUM_INC_LOW      0x89
#define AD5933_REG_SETTLING_HIGH    0x8A
#define AD5933_REG_SETTLING_LOW     0x8B
#define AD5933_REG_STATUS           0x8F
#define AD5933_REG_TEMP_DATA_HIGH   0x92
#define AD5933_REG_TEMP_DATA_LOW    0x93
#define AD5933_REG_REAL_DATA_HIGH   0x94
#define AD5933_REG_REAL_DATA_LOW    0x95
#define AD5933_REG_IMAG_DATA_HIGH   0x96
#define AD5933_REG_IMAG_DATA_LOW    0x97

// 控制命令
#define AD5933_CMD_INIT_START_FREQ  0x10
#define AD5933_CMD_START_FREQ_SWEEP 0x20
#define AD5933_CMD_INCREMENT_FREQ   0x30
#define AD5933_CMD_REPEAT_FREQ      0x40
#define AD5933_CMD_MEASURE_TEMP     0x90
#define AD5933_CMD_POWER_DOWN       0xA0
#define AD5933_CMD_STANDBY          0xB0

// 状态位
#define AD5933_STATUS_TEMP_VALID    0x01
#define AD5933_STATUS_DATA_VALID    0x02
#define AD5933_STATUS_SWEEP_DONE    0x04

// 阻抗测量结构体
typedef struct {
    int16_t Re;
    int16_t Im;
    float Impedance;
    float Phase;
} ImpeType;

// AD5933配置结构体
typedef struct {
    I2C_HandleTypeDef *hi2c;      // I2C句柄
    uint8_t device_addr;          // 设备地址
    uint8_t clk_source;           // 时钟源选择: 0x08 外部时钟, 0x00 内部时钟
    uint8_t gain;                 // 增益设置: 0x01 x1增益, 0x00 x5增益
    uint8_t output_range;         // 输出范围: 0x00(2Vpp), 0x02(0.2Vpp), 0x04(0.4Vpp), 0x06(1Vpp)
    uint32_t start_freq;          // 起始频率 (Hz)
    uint32_t freq_increment;      // 频率增量 (Hz)
    uint16_t num_increments;      // 扫描点数
    uint16_t settling_cycles;     // 稳定周期数
} AD5933_Config;

// 全局变量声明
extern AD5933_Config ad5933_config;

// 函数声明
HAL_StatusTypeDef AD5933_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef AD5933_WriteByte(uint8_t reg_addr, uint8_t data);
HAL_StatusTypeDef AD5933_ReadByte(uint8_t reg_addr, uint8_t *data);
HAL_StatusTypeDef AD5933_WriteWord(uint8_t reg_addr, uint16_t data);
HAL_StatusTypeDef AD5933_ReadWord(uint8_t reg_addr, uint16_t *data);

HAL_StatusTypeDef AD5933_FreInit(float freq_hz, float freq_increment_hz);
HAL_StatusTypeDef AD5933_StartTest(uint8_t add_ok);
HAL_StatusTypeDef AD5933_ReadImpedance(ImpeType *impedance_data);
HAL_StatusTypeDef AD5933_StartOnceTest(ImpeType *impedance_data, uint8_t add_ok);
float AD5933_Temperature_Test(void);

// 配置函数
void AD5933_SetConfig(uint8_t clk_source, uint8_t gain, uint8_t output_range);
void AD5933_SetFrequency(uint32_t start_freq, uint32_t freq_increment, uint16_t num_increments);
void AD5933_SetSettlingTime(uint16_t settling_cycles);

#endif /* AD5933_HAL_H */

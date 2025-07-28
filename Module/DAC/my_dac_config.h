#ifndef MY_DAC_CONFIG_H
#define MY_DAC_CONFIG_H

#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"
#include "my_parameter_config.h"  // 包含参数配置头文件

// DAC相关常量定义
#define DAC_MAX_VALUE 4095
#define DAC_REF_VOLTAGE 3.3f
#define VOLTAGE_TO_DAC_VALUE(voltage) ((uint16_t)((voltage) * DAC_MAX_VALUE / DAC_REF_VOLTAGE))

// 波形缓冲区
extern uint16_t* g_dac_waveform_buffer;

// 函数声明
void update_dac_waveform_by_parameters(void);
void generate_sine_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref);
void generate_square_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref);
void generate_triangle_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref);

#endif /* MY_DAC_CONFIG_H */
#ifndef MY_DAC_CONFIG_H
#define MY_DAC_CONFIG_H

#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"
#include "my_parameter_config.h"  // 包含参数配置头文件

// DAC相关常量定义
#define DAC_MAX_VALUE 4095
#define DAC_REF_VOLTAGE 3.44f
#define VOLTAGE_TO_DAC_VALUE(voltage) ((uint16_t)((voltage) * DAC_MAX_VALUE / DAC_REF_VOLTAGE))

float get_calibrated_AD9954_amplitude(float desired_vpp, float frequency_hz);

#endif /* MY_DAC_CONFIG_H */
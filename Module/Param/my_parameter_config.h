#ifndef MY_PARAMETER_CONFIG_H
#define MY_PARAMETER_CONFIG_H

#include "main.h"
#include "arm_math.h"
#include "my_freq_config.h"

// DDS相关参数
extern uint32_t g_desired_dds_frequency;
extern uint8_t g_desired_dds_type;
extern uint32_t g_desired_dds_phase;
extern uint32_t g_desired_dds_amplitude;

// ADC相关参数
extern uint32_t g_desired_ADC_sample_rate_Hz;

// DAC相关参数
extern uint8_t g_desired_DAC_output_waveform;
extern uint32_t g_desired_DAC_output_frequency;
extern float32_t g_desired_DAC_single_output_amplitude;

// 继电器控制参数
extern uint8_t g_desired_switch2which_relay;

// 功能状态参数
typedef enum {
    LCR_STATE = 0,
    SPECTRUM_STATE,
    TIME_STATE,
    DIY_STATE
} function_state_t;

extern function_state_t g_desired_function_state;

// 频谱数据缓冲区
extern float32_t g_adc1_spectrum_data[FFT_LENGTH / 2];
extern float32_t g_adc2_spectrum_data[FFT_LENGTH / 2];

#endif // MY_PARAMETER_CONFIG_H
#ifndef MY_PARAMETER_CONFIG_H
#define MY_PARAMETER_CONFIG_H

#include "main.h"
#include "arm_math.h"
#include "my_freq_config.h"

// DDS相关参数
// DDS类型定义
#define DDS_TYPE_AD9833 1
#define DDS_TYPE_AD9954 0
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
extern uint16_t* g_dac_waveform_buffer;
extern uint16_t g_dac_sine[256];
extern uint16_t g_dac_cosine[256];
extern uint16_t g_dac_square[256];
extern uint16_t g_dac_triangle[256];

// 25点波形数组声明
extern uint16_t g_dac_sine_25[25];
extern uint16_t g_dac_square_25[25];
extern uint16_t g_dac_triangle_25[25];

// 继电器控制参数
extern uint8_t g_desired_switch2which_relay;

// 功能状态参数
typedef enum {
    LCR_STATE = 0,
    SPECTRUM_STATE,
    TIME_STATE,
    TEST_DAC_DDS,
    DIY_STATE
} function_state_t;

extern function_state_t g_desired_function_state;

// 频谱数据缓冲区
extern float32_t g_adc1_spectrum_data[FFT_LENGTH / 2];
extern float32_t g_adc2_spectrum_data[FFT_LENGTH / 2];

#endif // MY_PARAMETER_CONFIG_H
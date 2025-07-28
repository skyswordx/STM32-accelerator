#include "my_parameter_config.h"

// DDS相关参数
uint32_t g_desired_dds_frequency = 1000;        // 默认1kHz
uint8_t g_desired_dds_type = 0;                 // 默认AD9954
uint32_t g_desired_dds_phase = 0;               // 默认0度
uint32_t g_desired_dds_amplitude = 16383;       // 默认最大幅度

// ADC相关参数
uint32_t g_desired_ADC_sample_rate_Hz = 2000000; // 默认2MHz

// DAC相关参数
uint8_t g_desired_DAC_output_waveform = 0;      // 默认正弦波
uint32_t g_desired_DAC_output_frequency = 1000; // 默认1kHz
float32_t g_desired_DAC_single_output_amplitude = 0.7f; // 默认0.7V

// 继电器控制参数
uint8_t g_desired_switch2which_relay = 0;       // 默认继电器0

// 功能状态参数
function_state_t g_desired_function_state = LCR_STATE; // 默认LCR状态

// 频谱数据缓冲区
float32_t g_adc1_spectrum_data[FFT_LENGTH / 2] = {0};
float32_t g_adc2_spectrum_data[FFT_LENGTH / 2] = {0};
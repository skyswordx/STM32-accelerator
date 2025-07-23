#include "my_freq_config.h"

#define FFT_LENGTH (4096) // FFT点数

extern uint32_t g_ADC_SAMPLE_RATE_Hz; // 100kHz采样率

float32_t g_fft_input_buffer[FFT_LENGTH * 2]; // FFT输入数组，大小为点数的两倍
float32_t g_fft_output_buffer[FFT_LENGTH];    // FFT输出数组，大小等于点数，调用 arm_cfft_f32 后存储的是模值大小


float32_t caculate_DCcomponent(float32_t* data, uint32_t length) {
    float32_t mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        mean += data[i];
    }
    mean /= length;
    return mean;
}

/**
 * @brief 应用FFT算法
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param fft_instance FFT实例结构体指针
 * @retval None
 * @note 该函数依赖两个全局缓冲区
 */
void my_armcfft32_apply(float32_t* adc_input, const arm_cfft_radix4_instance_f32* fft_instance, fundamental_result_t* result)
{
    uint16_t fftLen = fft_instance->fftLen;
    uint16_t n;
    float32_t mean = caculate_DCcomponent(adc_input, fftLen);

    for( n = 0; n < fftLen; n++) {
        // 将输入数据转换为复数格式，实部在偶数索引，虚部在奇数索引
        g_fft_input_buffer[2 * n] = (adc_input[n] - mean); // 实部
        g_fft_input_buffer[2 * n + 1] = 0.0f;     // 虚部
    }

    // 执行FFT
    arm_cfft_radix4_f32(fft_instance, g_fft_input_buffer);
    // 计算模值
    arm_cmplx_mag_f32(g_fft_input_buffer, g_fft_output_buffer, fftLen);

    //在模值中寻找基波分量
    float32_t fundamental_magnitude = 0.0f; // 基波幅度
    uint16_t fundamental_index = 0; // 基波索引

    for (uint16_t i = 0; i < fftLen / 2 - 1; i++) {
        if (g_fft_output_buffer[i] > fundamental_magnitude) {
            fundamental_index = i;
            fundamental_magnitude = g_fft_output_buffer[i];
        }
    }

    float32_t fundamental_phase_angle = (atan2f(g_fft_input_buffer[2 * fundamental_index + 1], g_fft_input_buffer[2 * fundamental_index])) * (180.0f / PI) ;

    result->fundamental_vpp = (fundamental_magnitude) * 2.0f / fftLen; // 基波峰峰值
    result->fundamental_vrms = result->fundamental_vpp / sqrtf(2.0f); // 基波有效值
    result->fundamental_frequency = fundamental_index * (g_ADC_SAMPLE_RATE_Hz / fftLen); // 假设采样率为100kHz
    result->fundamental_phase_angle = fundamental_phase_angle;

}
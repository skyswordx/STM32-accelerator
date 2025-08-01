#include "filter_imitate.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.141592653589793f
#endif

// 内部状态变量
static filter_imitate_state_t g_imitate_state = IMITATE_STATE_IDLE;
static ContinuousTransferFunction g_stored_tf;

// FFT实例和缓冲区
static arm_cfft_radix4_instance_f32 g_cfft_instance;
static float32_t g_fft_input_buffer[FFT_LENGTH * 2];  // 复数FFT输入缓冲区 [实部,虚部,实部,虚部...]
static float32_t g_fft_output_buffer[FFT_LENGTH * 2]; // 复数FFT输出缓冲区

// 初始化模块
void filter_imitate_init(void)
{
    g_imitate_state = IMITATE_STATE_IDLE;
    
    // 初始化FFT实例
    arm_cfft_radix4_init_f32(&g_cfft_instance, FFT_LENGTH, 0, 1); // 正向FFT
    
    // 清零缓冲区
    memset(g_fft_input_buffer, 0, sizeof(g_fft_input_buffer));
    memset(g_fft_output_buffer, 0, sizeof(g_fft_output_buffer));
    
    // 清零传递函数
    memset(&g_stored_tf, 0, sizeof(g_stored_tf));
}

// 设置学习到的传递函数
void filter_imitate_set_transfer_function(const ContinuousTransferFunction* tf)
{
    if (tf == NULL) {
        g_imitate_state = IMITATE_STATE_IDLE;
        return;
    }
    
    // 复制传递函数
    memcpy(&g_stored_tf, tf, sizeof(ContinuousTransferFunction));
    
    // 检查传递函数有效性
    if (isnan(g_stored_tf.b0) || isnan(g_stored_tf.a0)) {
        g_imitate_state = IMITATE_STATE_IDLE;
        return;
    }
    
    g_imitate_state = IMITATE_STATE_READY;
}

// 计算传递函数在给定频率处的复数响应
static void compute_transfer_function_response(float32_t frequency_hz, 
                                              uint32_t sampling_rate,
                                              float32_t* h_real, 
                                              float32_t* h_imag)
{
    // 计算角频率
    float32_t omega = 2.0f * PI * frequency_hz;
    
    // 计算 s = jω
    float32_t s_real = 0.0f;
    float32_t s_imag = omega;
    
    // 计算分子 N(s) = b2*s^2 + b1*s + b0
    float32_t s2_real = s_real * s_real - s_imag * s_imag; // s^2的实部
    float32_t s2_imag = 2.0f * s_real * s_imag;            // s^2的虚部
    
    float32_t num_real = g_stored_tf.b2 * s2_real + g_stored_tf.b1 * s_real + g_stored_tf.b0;
    float32_t num_imag = g_stored_tf.b2 * s2_imag + g_stored_tf.b1 * s_imag;
    
    // 计算分母 D(s) = s^2 + a1*s + a0
    float32_t den_real = s2_real + g_stored_tf.a1 * s_real + g_stored_tf.a0;
    float32_t den_imag = s2_imag + g_stored_tf.a1 * s_imag;
    
    // 计算 H(s) = N(s) / D(s) = (num_real + j*num_imag) / (den_real + j*den_imag)
    float32_t den_mag_sq = den_real * den_real + den_imag * den_imag;
    
    if (den_mag_sq < 1e-12f) {
        // 避免除以零
        *h_real = 0.0f;
        *h_imag = 0.0f;
        return;
    }
    
    *h_real = (num_real * den_real + num_imag * den_imag) / den_mag_sq;
    *h_imag = (num_imag * den_real - num_real * den_imag) / den_mag_sq;
}

// 处理输入信号并生成输出信号
int filter_imitate_process_signal(const float32_t* input_signal, 
                                  float32_t* output_signal, 
                                  uint32_t sampling_rate)
{
    if (g_imitate_state != IMITATE_STATE_READY && g_imitate_state != IMITATE_STATE_ACTIVE) {
        return -1; // 传递函数未就绪
    }
    
    if (input_signal == NULL || output_signal == NULL) {
        return -1; // 无效参数
    }
    
    g_imitate_state = IMITATE_STATE_ACTIVE;
    
    // 1. 准备复数FFT输入（实部=输入信号，虚部=0）
    for (uint32_t i = 0; i < FFT_LENGTH; i++) {
        g_fft_input_buffer[i * 2]     = input_signal[i]; // 实部
        g_fft_input_buffer[i * 2 + 1] = 0.0f;            // 虚部
    }
    
    // 2. 进行正向FFT
    arm_cfft_radix4_f32(&g_cfft_instance, g_fft_input_buffer);
    
    // 3. 在频域应用传递函数
    float32_t freq_resolution = (float32_t)sampling_rate / (float32_t)FFT_LENGTH;
    
    for (uint32_t k = 0; k < FFT_LENGTH; k++) {
        float32_t frequency = k * freq_resolution;
        
        // 处理Nyquist频率折叠
        if (k > FFT_LENGTH / 2) {
            frequency = (float32_t)sampling_rate - frequency;
        }
        
        // 计算传递函数响应
        float32_t h_real, h_imag;
        compute_transfer_function_response(frequency, sampling_rate, &h_real, &h_imag);
        
        // 获取输入频谱
        float32_t x_real = g_fft_input_buffer[k * 2];
        float32_t x_imag = g_fft_input_buffer[k * 2 + 1];
        
        // 复数乘法: Y(k) = X(k) * H(k)
        g_fft_output_buffer[k * 2]     = x_real * h_real - x_imag * h_imag; // 实部
        g_fft_output_buffer[k * 2 + 1] = x_real * h_imag + x_imag * h_real; // 虚部
    }
    
    // 4. 进行逆向FFT
    arm_cfft_radix4_instance_f32 ifft_instance;
    arm_cfft_radix4_init_f32(&ifft_instance, FFT_LENGTH, 1, 1); // 逆向FFT
    arm_cfft_radix4_f32(&ifft_instance, g_fft_output_buffer);
    
    // 5. 提取实部作为输出信号，并进行归一化
    for (uint32_t i = 0; i < FFT_LENGTH; i++) {
        output_signal[i] = g_fft_output_buffer[i * 2] / (float32_t)FFT_LENGTH;
    }
    
    return 0; // 成功
}

// 获取当前模仿状态
filter_imitate_state_t filter_imitate_get_state(void)
{
    return g_imitate_state;
}

// 重置模仿状态
void filter_imitate_reset(void)
{
    g_imitate_state = IMITATE_STATE_IDLE;
    memset(&g_stored_tf, 0, sizeof(g_stored_tf));
}

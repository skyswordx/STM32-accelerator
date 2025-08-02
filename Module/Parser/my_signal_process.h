#ifndef MY_SIGNAL_PROCESSING_H
#define MY_SIGNAL_PROCESSING_H

#include "arm_math.h"
#include "my_freq_config.hh" // 为了引用 FFT_LENGTH
#include "filter_identification.h" // 为了引用 ContinuousTransferFunction

// --- 谐波分析部分 (要求 2) ---

#define MAX_HARMONICS 20 // 定义最多分析的谐波数量

/**
 * @brief 存储单个频率分量（基波或谐波）的信息
 */
typedef struct {
    float32_t frequency;    // 频率 (Hz)
    float32_t amplitude;    // 幅度 (V, 注意不是Vpp)
    float32_t phase;        // 相位 (radians)
    uint16_t  fft_index;    // 在FFT结果中的索引
} harmonic_component_t;


/**
 * @brief 分析输入信号，提取基波和谐波分量
 * @param[in]  adc_input 输入的ADC采样数据
 * @param[out] components 存储分析结果的谐波分量数组
 * @param[in]  max_components 结果数组的最大容量
 * @param[in]  fft_input_buffer 用于FFT计算的复数输入缓冲区 (FFT_LENGTH * 2)
 * @param[in]  fft_output_buffer 用于存储FFT幅度结果的缓冲区 (FFT_LENGTH)
 * @return int32_t 实际找到的谐波数量 (包括基波)
 */
int32_t analyze_signal_with_harmonics(
    float32_t* adc_input, 
    harmonic_component_t* components, 
    int32_t max_components,
    float32_t* fft_input_buffer,
    float32_t* fft_output_buffer
);


// --- 信号重建部分 (要求 3) ---

#define WAVEFORM_RECONSTRUCTION_POINTS 1024 // 定义重建波形的采样点数

/**
 * @brief 在测量的频响数据中通过线性插值查找指定频率的响应
 * @param[out] H_cmplx_out 插值计算得到的复数响应 [Real, Imag]
 * @param[in]  target_freq_hz 需要计算响应的目标频率
 * @param[in]  w_rad_sweep 扫频时记录的角频率数组 (rad/s)
 * @param[in]  H_measured_cmplx 扫频时记录的复数响应数组 [R,I,R,I...]
 * @param[in]  num_sweep_points 扫频点的数量
 * @return arm_status ARM_MATH_SUCCESS 如果成功, ARM_MATH_ARGUMENT_ERROR 如果频率超出范围
 */
arm_status get_system_response_at_freq(
    float32_t* H_cmplx_out,
    float32_t target_freq_hz,
    const float32_t* w_rad_sweep,
    const float32_t* H_measured_cmplx,
    uint32_t num_sweep_points
);

/**
 * @brief 根据输入信号的谐波分量和系统的频响特性，重建时域输出波形
 * @param[out] reconstructed_waveform 存储重建波形的输出数组
 * @param[in]  waveform_points 重建波形的点数
 * @param[in]  input_components 输入信号的谐波分量数组
 * @param[in]  num_components 谐波分量的数量
 * @param[in]  w_rad_sweep 扫频时记录的角频率数组 (rad/s)
 * @param[in]  H_measured_cmplx 扫频时记录的复数响应数组 [R,I,R,I...]
 * @param[in]  num_sweep_points 扫频点的数量
 */
void reconstruct_output_waveform(
    float32_t* reconstructed_waveform,
    uint32_t waveform_points,
    const harmonic_component_t* input_components,
    int32_t num_components,
    const float32_t* w_rad_sweep,
    const float32_t* H_measured_cmplx,
    uint32_t num_sweep_points
);

#endif /* MY_SIGNAL_PROCESSING_H */
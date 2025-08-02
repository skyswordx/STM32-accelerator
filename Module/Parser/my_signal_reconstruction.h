#ifndef MY_SIGNAL_RECONSTRUCTION_H
#define MY_SIGNAL_RECONSTRUCTION_H

#include "arm_math.h"
#include "my_freq_config.h" // 引用 FFT_LENGTH 和其他定义
#include "stdint.h"         // 引入 uint8_t

// --- 数据结构定义 ---

#define MAX_HARMONICS 25 // 定义最多分析的谐波数量
#define WAVEFORM_RECONSTRUCTION_POINTS 64 // 定义重建波形的采样点数

/**
 * @brief 存储单个频率分量（基波或谐波）的信息
 */
typedef struct {
    float32_t frequency;    // 频率 (Hz)
    float32_t amplitude;    // 幅度 (V, 峰值)
    float32_t phase;        // 相位 (radians)
} harmonic_component_t;

/**
 * @brief 定义标准波形类型
 */
typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_UNKNOWN // 用于表示非标准波形
} wave_type_t;

/**
 * @brief 定义最终选择的分析方法
 */
typedef enum {
    METHOD_FAILED = 0,
    METHOD_MATH_SYNTHESIS = 1,
    METHOD_GENERIC_FFT = 2 // 简化为两级回退
} analysis_method_t;


// --- 函数声明 ---

/**
 * @brief 顶层控制函数：分析输入信号，并根据使能开关选择最佳谐波模型
 * @param[out] final_components 输出最终选择的谐波分量数组
 * @param[out] num_final_components 输出谐波分量的数量
 * @param[in]  adc_input 输入的ADC采样数据
 * @param[in]  enable_math_synthesis 是否允许尝试使用数学合成法 (1=是, 0=否)
 * @return analysis_method_t 返回最终采用的分析方法
 */
analysis_method_t analyze_and_select_best_method(
    harmonic_component_t* final_components,
    int32_t* num_final_components,
    float32_t* adc_input,
    uint8_t enable_math_synthesis
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

#endif /* MY_SIGNAL_RECONSTRUCTION_H */

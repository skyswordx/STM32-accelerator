#ifndef MY_FREQ_CONFIG_H
#define MY_FREQ_CONFIG_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "arm_math.h"
#include "my_adc_task.h"

#define FFT_LENGTH (4096) // FFT点数

// --- 频谱插值模式定义 ---
typedef enum {
    INTERPOLATION_DISABLED = 0,      // 禁用插值
    INTERPOLATION_PARABOLIC = 1,     // 启用二次抛物线插值 (通用方法)
    INTERPOLATION_HANNING_SPECIAL = 2  // 启用汉宁窗专用插值 (理论上更精确)
} spectral_interpolation_mode_t;

// 用于存储插值计算结果的内部结构体
typedef struct {
    float32_t corrected_frequency;
    float32_t corrected_magnitude;
} interpolated_peak_t;

typedef struct {
    float32_t fundamental_vpp;           // 基波峰峰值
    float32_t fundamental_vrms;          // 基波有效值
    uint16_t fundamental_frequency;       // 基波频率
    float32_t fundamental_phase_angle;  // 基波相位（角度）
} fundamental_result_t;

void my_armcfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, uint8_t enable_window, spectral_interpolation_mode_t interpolation_mode);
void my_armrfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, uint8_t enable_window, spectral_interpolation_mode_t interpolation_mode);

/* FIR 低通滤波器 + 窗函数 */
#define FIR_ORDER   100
#define NUM_TAPS    (FIR_ORDER + 1) // number of taps

#define HANNING_WINDOW_FACTOR 2.0f // 汉宁窗补偿系数 2.325475f



#endif /* MY_FREQ_CONFIG_H */
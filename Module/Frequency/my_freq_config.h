#ifndef MY_FREQ_CONFIG_H
#define MY_FREQ_CONFIG_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "arm_math.h"

// Quinn频率估计算法支持
#ifdef ENABLE_QUINN_FREQUENCY_ESTIMATION
#include "my_quinn_config.h"
#endif

#define FFT_LENGTH (4096) // FFT点数

// --- 窗函数类型定义 ---
/**
 * @brief 窗函数类型枚举
 *
 * 窗函数用于减少频谱泄漏，提高频率和幅度测量精度。
 * 不同的窗函数有不同的特性，适用于不同的应用场景：
 *
 * WINDOW_NONE - 不应用任何窗函数，适合信号频率恰好与FFT频率栅格对齐的情况
 * WINDOW_HANNING - 汉宁窗，通用窗函数，平衡频率分辨率和幅度精度
 * WINDOW_FLAT_TOP - 平顶窗，专为高精度幅度测量设计，幅度波动小于0.1dB
 */
typedef enum {
    WINDOW_NONE = 0,        // 无窗函数
    WINDOW_HANNING = 1,     // 汉宁窗
    WINDOW_FLAT_TOP = 2     // 平顶窗
} window_type_t;

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
    uint32_t fundamental_frequency;       // 基波频率
    float32_t fundamental_phase;  // 基波相位（弧度）
} fundamental_result_t;

/**
 * @brief 应用复数FFT算法进行频谱分析
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param window_type 窗函数类型:
 *                    - WINDOW_NONE: 不应用窗函数
 *                    - WINDOW_HANNING: 应用汉宁窗
 *                    - WINDOW_FLAT_TOP: 应用平顶窗(适合高精度幅度测量)
 * @param interpolation_mode 频谱插值模式:
 *                          - INTERPOLATION_DISABLED: 不使用插值
 *                          - INTERPOLATION_PARABOLIC: 二次抛物线插值
 *                          - INTERPOLATION_HANNING_SPECIAL: 汉宁窗专用插值
 */
float32_t my_armcfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, window_type_t window_type, spectral_interpolation_mode_t interpolation_mode);


/* FIR 低通滤波器 + 窗函数 */
#define FIR_ORDER   100
#define NUM_TAPS    (FIR_ORDER + 1) // number of taps

#define HANNING_WINDOW_FACTOR 2.0f   // 汉宁窗补偿系数
#define FLAT_TOP_WINDOW_FACTOR 0.216f // 平顶窗补偿系数 (ISO 18431-2标准)

float32_t caculate_DCcomponent(float32_t* data, uint32_t length);

#endif /* MY_FREQ_CONFIG_H */
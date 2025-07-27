#ifndef MY_TIME_DETECT_H
#define MY_TIME_DETECT_H

#include "main.h"
#include "arm_math.h" // ARM CMSIS-DSP 库
#include <stdint.h>   // 标准整数类型定义

// 时域检测配置参数
typedef struct {
    uint32_t sample_rate;     // 采样率
    uint32_t data_length;     // 数据长度
    uint8_t enable_dc_filter; // 是否启用直流分量滤除
} time_detect_config_params_t;

// 时域检测结果
typedef struct {
    float32_t rms_value;         // 有效值
    float32_t dc_component;      // 直流分量
    uint32_t fundamental_freq;   // 基波频率
    uint32_t period_points;      // 一个周期的点数
    float32_t waveform_ratio;    // 波形特征比值（用于判断波形类型）
    int waveform_type;           // 波形类型（0:未知, 1:正弦波, 2:方波, 3:三角波）
} time_detect_result_t;

// 波形类型定义
#define WAVEFORM_UNKNOWN 0
#define WAVEFORM_SINE 1
#define WAVEFORM_SQUARE 2
#define WAVEFORM_TRIANGLE 3

// 函数接口声明
void my_time_detect_init(time_detect_config_params_t* config);
int my_time_detect_start(float32_t* data, time_detect_result_t* result);

#endif // MY_TIME_DETECT_H
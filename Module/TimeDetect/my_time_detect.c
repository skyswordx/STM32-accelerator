#include "my_time_detect.h"
#include "my_freq_config.h"  // 引入频域处理模块
#include "arm_math.h"
#include "main.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

// 全局配置参数
time_detect_config_params_t g_time_config;

// 滤除直流分量
float32_t calculate_DCcomponent(float32_t* data, uint32_t length) {
    float32_t mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        mean += data[i];
    }
    mean /= length;
    return mean;
}

// 初始化时域检测模块
void my_time_detect_init(time_detect_config_params_t* config) {
    if (config != NULL) {
        g_time_config.sample_rate = config->sample_rate;
        g_time_config.data_length = config->data_length;
        g_time_config.enable_dc_filter = config->enable_dc_filter;
        printf("Time Detect Module Initialized\n");
    }
}

// 获取一个周期的点数
uint32_t get_period_points(uint32_t fundamental_freq, uint32_t sample_rate) {
    if (fundamental_freq == 0) {
        return 0;
    }
    return sample_rate / fundamental_freq;
}

// 判断波形类型
int detect_waveform_type(float32_t* data, uint32_t period_points, float32_t rms_value, float32_t amplitude) {
    if (period_points == 0 || amplitude == 0.0f) {
        return WAVEFORM_UNKNOWN;
    }
    
    // 计算理论RMS与实际RMS的比值
    float32_t ratio = rms_value / amplitude;
    
    // 根据比值判断波形类型
    // 正弦波: 0.707, 方波: 1.0, 三角波: 0.577
    if (ratio > 0.65f && ratio < 0.75f) {
        return WAVEFORM_SINE;      // 正弦波
    } else if (ratio > 0.9f && ratio < 1.1f) {
        return WAVEFORM_SQUARE;    // 方波
    } else if (ratio > 0.5f && ratio < 0.65f) {
        return WAVEFORM_TRIANGLE;  // 三角波
    }
    
    return WAVEFORM_UNKNOWN;       // 未知波形
}

// 启动时域检测
int my_time_detect_start(float32_t* data, time_detect_result_t* result) {
    printf("my_time_detect_start: Starting time domain detection\n");
    
    if (data == NULL || result == NULL) {
        printf("my_time_detect_start: Parameter error - data or result is NULL\n");
        return -1; // 参数错误
    }
    
    printf("my_time_detect_start: Parameters validated\n");
    
    // 初始化结果结构体
    memset(result, 0, sizeof(time_detect_result_t));
    
    // 1. 计算直流分量
    float32_t dc_component = calculate_DCcomponent(data, g_time_config.data_length);
    result->dc_component = dc_component;
    printf("my_time_detect_start: DC component calculated: %.6f\n", dc_component);
    
    // 2. 滤除直流分量（如果启用）
    float32_t* filtered_data = (float32_t*)malloc(g_time_config.data_length * sizeof(float32_t));
    if (filtered_data == NULL) {
        printf("my_time_detect_start: Memory allocation failed\n");
        return -2; // 内存分配失败
    }
    printf("my_time_detect_start: Memory allocated for filtered data\n");
    
    if (g_time_config.enable_dc_filter) {
        for (uint32_t i = 0; i < g_time_config.data_length; i++) {
            filtered_data[i] = data[i] - dc_component;
        }
        printf("my_time_detect_start: DC component filtered from data\n");
    } else {
        memcpy(filtered_data, data, g_time_config.data_length * sizeof(float32_t));
        printf("my_time_detect_start: Data copied without DC filtering\n");
    }
    
    // 3. 使用频域处理模块获取基波频率和其他参数
    fundamental_result_t freq_result;
    printf("my_time_detect_start: Calling my_armcfft32_apply\n");
    // 调用频域处理模块的函数，使用汉宁窗和汉宁窗专用插值
    my_armcfft32_apply(filtered_data, &freq_result, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
    printf("my_time_detect_start: my_armcfft32_apply completed\n");
    
    // 4. 获取基波频率
    result->fundamental_freq = freq_result.fundamental_frequency;
    printf("my_time_detect_start: Fundamental frequency detected: %lu Hz\n", result->fundamental_freq);
    
    // 5. 计算一个周期的点数
    result->period_points = get_period_points(result->fundamental_freq, g_time_config.sample_rate);
    printf("my_time_detect_start: Period points calculated: %lu\n", result->period_points);
    
    // 6. 如果能确定周期点数，则只使用一个周期的数据计算RMS
    uint32_t points_to_use = g_time_config.data_length;
    if (result->period_points > 0 && result->period_points < g_time_config.data_length) {
        points_to_use = result->period_points;
    }
    printf("my_time_detect_start: Points to use for RMS calculation: %lu\n", points_to_use);
    
    // 7. 计算RMS值
    arm_rms_f32(filtered_data, points_to_use, &result->rms_value);
    printf("my_time_detect_start: RMS value calculated: %.6f\n", result->rms_value);
    
    // 8. 计算幅度（峰峰值的一半）
    float32_t max_val, min_val;
    uint32_t max_index, min_index;
    arm_max_f32(filtered_data, points_to_use, &max_val, &max_index);
    arm_min_f32(filtered_data, points_to_use, &min_val, &min_index);
    float32_t amplitude = (max_val - min_val) / 2.0f;
    printf("my_time_detect_start: Amplitude calculated: %.6f (max: %.6f, min: %.6f)\n", amplitude, max_val, min_val);
    
    // 9. 判断波形类型
    result->waveform_type = detect_waveform_type(filtered_data, result->period_points, result->rms_value, amplitude);
    printf("my_time_detect_start: Waveform type detected: %d\n", result->waveform_type);
    
    // 10. 计算波形特征比值
    if (amplitude > 0.0f) {
        result->waveform_ratio = result->rms_value / amplitude;
        printf("my_time_detect_start: Waveform ratio calculated: %.6f\n", result->waveform_ratio);
    }
    
    // 释放内存
    free(filtered_data);
    printf("my_time_detect_start: Memory freed, detection completed successfully\n");
    
    return 0; // 成功
}
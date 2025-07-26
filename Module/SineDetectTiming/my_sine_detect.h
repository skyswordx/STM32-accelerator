#ifndef MY_SINE_DETECT_H
#define MY_SINE_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"

// 峰值检测结果结构体
typedef struct {
    float32_t amplitude;     // 峰值幅度
    float32_t position;      // 峰值位置（亚采样点精度）
    uint32_t index;          // 峰值索引（整数位置）
    uint8_t is_positive;     // 是否为正峰值（1=正峰值，0=负峰值）
} peak_result_t;

// 边沿检测结果结构体
typedef struct {
    float32_t position;      // 边沿位置（亚采样点精度）
    uint32_t index;          // 边沿索引（整数位置）
    uint8_t is_rising;       // 是否为上升沿（1=上升沿，0=下降沿）
} edge_result_t;

// 模块配置结构体
typedef struct {
    uint32_t sample_rate;    // 采样率(Hz)
    uint32_t data_length;    // 数据长度
    uint8_t enable_filter;   // 是否启用滤波（1=启用，0=禁用）
    uint32_t filter_length;  // 滤波器长度
} sine_detect_config_t;

// 模块结果结构体
typedef struct {
    peak_result_t* peaks;    // 峰值结果数组
    uint32_t peak_count;     // 峰值数量
    edge_result_t* edges;    // 边沿结果数组
    uint32_t edge_count;     // 边沿数量
    float32_t signal_midpoint; // 信号中点
} sine_detect_result_t;

/**
 * @brief 初始化正弦波检测模块
 * @param config 模块配置参数
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_init(const sine_detect_config_t* config);

/**
 * @brief 处理ADC数据，检测峰值和边沿
 * @param adc_data 输入的ADC数据数组
 * @param result 输出的检测结果
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_process(const float32_t* adc_data, sine_detect_result_t* result);

/**
 * @brief 获取峰值检测结果
 * @param peaks 峰值结果数组
 * @param max_count 最大峰值数量
 * @return 实际检测到的峰值数量
 */
uint32_t my_sine_detect_get_peaks(peak_result_t* peaks, uint32_t max_count);

/**
 * @brief 获取边沿检测结果
 * @param edges 边沿结果数组
 * @param max_count 最大边沿数量
 * @return 实际检测到的边沿数量
 */
uint32_t my_sine_detect_get_edges(edge_result_t* edges, uint32_t max_count);

/**
 * @brief 获取信号中点
 * @return 信号中点值
 */
float32_t my_sine_detect_get_midpoint(void);

/**
 * @brief 释放模块资源
 */
void my_sine_detect_deinit(void);

#endif // MY_SINE_DETECT_H
#ifndef MY_TIME_DETECT_H
#define MY_TIME_DETECT_H

#include "main.h"
#include "arm_math.h" // ARM CMSIS-DSP 库
#include <stdint.h>   // 标准整数类型定义

/**
 * @brief 时域分析的配置参数结构体
 */
typedef struct {
    uint8_t   enable_filter;      // 是否启用IIR低通滤波器 (1=启用, 0=禁用)
    float32_t filter_alpha;       // IIR滤波器系数 (例如 0.05f), enable_filter为1时有效
    float32_t hysteresis_v;       // 频率检测的迟滞电压 (V), 用于抑制噪声 (例如 0.05f)
} time_domain_config_t;

/**
 * @brief 时域分析结果结构体
 */
typedef struct {
    float32_t dc_offset;    // 直流分量 (V)
    float32_t ac_rms;       // 交流有效值 (V)
    float32_t vpp_peak;     // 通过峰值检测得到的Vpp (V_max - V_min)
    float32_t v_max;        // 通过峰值检测得到的最大电压 (V)
    float32_t v_min;        // 通过峰值检测得到的最小电压 (V)
    float32_t frequency;    // 通过带迟滞和线性插值的过零检测法测得的频率 (Hz)
} time_domain_result_t;


/**
 * @brief 对ADC采样数据进行优化的时域分析
 * @param data_in 指向输入的数据缓冲区 (浮点电压值)
 * @param temp_buffer 指向一个与data_in同样大小的临时缓冲区，用于滤波和内部计算
 * @param size 采样点数量
 * @param sample_rate_hz ADC的采样率 (Hz)
 * @param config 指向时域分析的配置参数
 * @param result_out 指向用于存储分析结果的结构体
 */
void my_time_domain_analysis_optimized(const float32_t* data_in, 
                                       float32_t* temp_buffer, 
                                       uint32_t size, 
                                       uint32_t sample_rate_hz, 
                                       const time_domain_config_t* config, 
                                       time_domain_result_t* result_out);

#endif // MY_TIME_DETECT_H

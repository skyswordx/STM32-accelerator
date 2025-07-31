#ifndef MY_TIME_DETECT_H
#define MY_TIME_DETECT_H

#include "main.h"
#include "arm_math.h" // ARM CMSIS-DSP 库
#include <stdint.h>   // 标准整数类型定义


/**
 * @brief 时域分析结果结构体 (已更新)
 */
typedef struct {
    float32_t dc_offset;    // 直流分量 (V)
    float32_t ac_rms;       // 交流有效值 (V)
    float32_t vpp_sine;     // 基于RMS计算出的正弦波Vpp (V)
    float32_t vpp_square;   // 基于RMS计算出的方波Vpp (V)
    float32_t v_max;        // 通过峰值检测得到的最大电压 (V)
    float32_t v_min;        // 通过峰值检测得到的最小电压 (V)
    float32_t vpp_peak;     // 通过峰值检测得到的Vpp (V_max - V_min)
    
    // --- 新增频率检测结果 ---
    float32_t frequency;    // 通过过零检测法测得的频率 (Hz)

} time_domain_result_t;


/**
 * @brief 对ADC采样数据进行时域分析 (包括幅度和频率)
 * @param data_in 指向输入的数据缓冲区 (浮点电压值)
 * @param size 采样点数量
 * @param sample_rate_hz ADC的采样率 (Hz), 用于计算频率
 * @param result_out 指向用于存储分析结果的结构体
 */
void my_time_domain_analysis(const float32_t* data_in, uint32_t size, uint32_t sample_rate_hz, time_domain_result_t* result_out);


#endif // MY_TIME_DETECT_H

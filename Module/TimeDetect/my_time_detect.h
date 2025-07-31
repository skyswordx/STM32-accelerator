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
 * (此函数保持不变)
 */
void my_time_domain_analysis_optimized(const float32_t* data_in, 
                                       float32_t* temp_buffer, 
                                       uint32_t size, 
                                       uint32_t sample_rate_hz, 
                                       const time_domain_config_t* config, 
                                       time_domain_result_t* result_out);

/**
 * @brief [新增] 提取整数个周期并重采样到指定长度 (用于FFT前处理)
 * @param data_in 指向原始的ADC数据缓冲区
 * @param in_size 原始数据的大小 (例如 4096)
 * @param data_out 指向用于存放重采样后数据的输出缓冲区
 * @param out_size 期望的输出大小，必须是2的次幂 (例如 1024, 4096)
 * @param config 指向时域分析的配置参数 (用于过零检测)
 * @return 1 表示成功提取并重采样, 0 表示失败 (未找到足够的周期)
 */
int my_resample_integer_cycles(const float32_t* data_in, 
                               uint32_t in_size, 
                               float32_t* data_out, 
                               uint32_t out_size,
                               const time_domain_config_t* config);

#endif // MY_TIME_DETECT_H

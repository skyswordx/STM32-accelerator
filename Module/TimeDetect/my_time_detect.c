#include "my_time_detect.h"
#include "arm_math.h"
#include "main.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h> // For NULL

/**
 * @brief 对ADC采样数据进行时域分析
 * @param data_in 指向输入的数据缓冲区 (浮点电压值)
 * @param size 采样点数量
 * @param result_out 指向用于存储分析结果的结构体
 */
void my_time_domain_analysis(const float32_t* data_in, uint32_t size, time_domain_result_t* result_out)
{
    if (data_in == NULL || result_out == NULL || size == 0) {
        return; // 安全检查，防止空指针或无效尺寸
    }

    float64_t sum = 0.0; // 使用双精度来累加，防止溢出，提高精度

    // 初始化峰值检测变量
    result_out->v_max = data_in[0];
    result_out->v_min = data_in[0];

    // --- 步骤 1: 计算直流分量 (平均值)，同时完成峰值检测 ---
    for (uint32_t i = 0; i < size; i++) {
        sum += data_in[i];
        if (data_in[i] > result_out->v_max) {
            result_out->v_max = data_in[i];
        }
        if (data_in[i] < result_out->v_min) {
            result_out->v_min = data_in[i];
        }
    }
    result_out->dc_offset = (float32_t)(sum / size);
    result_out->vpp_peak = result_out->v_max - result_out->v_min; // 直接计算峰峰值

    // --- 步骤 2: 计算交流分量的RMS值 ---
    sum = 0.0; // 重置sum用于计算平方和
    for (uint32_t i = 0; i < size; i++) {
        // 从每个样本中减去直流分量，得到纯交流分量
        float32_t ac_sample = data_in[i] - result_out->dc_offset;
        // 计算平方和
        sum += ac_sample * ac_sample;
    }
    // 计算均方根 (RMS)
    result_out->ac_rms = sqrtf((float32_t)(sum / size));

    // --- 步骤 3: 根据RMS值推算Vpp ---
    // 对于正弦波: Vpp = Vrms * 2 * sqrt(2)
    result_out->vpp_sine = result_out->ac_rms * 2.0f * 1.41421356f;
    
    // 对于50%占空比方波: Vpp = Vrms * 2
    result_out->vpp_square = result_out->ac_rms * 2.0f;
}
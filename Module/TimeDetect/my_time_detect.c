#include "my_time_detect.h"
#include "arm_math.h"
#include "main.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h> // For NULL

// --- FIR滤波器系数 (从MATLAB导出，时间反转) ---
// CMSIS-DSP FIR函数要求系数是时间反转的 (用MATLAB的fliplr(b)获得)
const float32_t fir_2M_60k[101] = {
  -0.0000000000f,  -0.0005000455f,  +0.0003260878f,  +0.0003510282f,
  -0.0006218246f,  +0.0000000000f,  +0.0007729541f,  -0.0005387764f,
  -0.0006100689f,  +0.0011198434f,  -0.0000000000f,  -0.0014409358f,
  +0.0010081104f,  +0.0011387226f,  -0.0020760219f,  -0.0000000000f,
  +0.0026144121f,  -0.0018058351f,  -0.0020128676f,  +0.0036210368f,
  -0.0000000000f,  -0.0044444914f,  +0.0030334648f,  +0.0033436719f,
  -0.0059533604f,  +0.0000000000f,  +0.0071784047f,  -0.0048635073f,
  -0.0053273999f,  +0.0094370874f,  -0.0000000000f,  -0.0113061549f,
  +0.0076515583f,  +0.0083851325f,  -0.0148861921f,  +0.0000000000f,
  +0.0180237354f,  -0.0123082365f,  -0.0136526346f,  +0.0246247806f,
  -0.0000000000f,  -0.0312395004f,  +0.0220646149f,  +0.0255674894f,
  -0.0488431839f,  +0.0000000000f,  +0.0746162882f,  -0.0618804948f,
  -0.0932437971f,  +0.3025668540f,  +0.6002201035f,  +0.3025668540f,
  -0.0932437971f,  -0.0618804948f,  +0.0746162882f,  +0.0000000000f,
  -0.0488431839f,  +0.0255674894f,  +0.0220646149f,  -0.0312395004f,
  -0.0000000000f,  +0.0246247806f,  -0.0136526346f,  -0.0123082365f,
  +0.0180237354f,  +0.0000000000f,  -0.0148861921f,  +0.0083851325f,
  +0.0076515583f,  -0.0113061549f,  -0.0000000000f,  +0.0094370874f,
  -0.0053273999f,  -0.0048635073f,  +0.0071784047f,  +0.0000000000f,
  -0.0059533604f,  +0.0033436719f,  +0.0030334648f,  -0.0044444914f,
  -0.0000000000f,  +0.0036210368f,  -0.0020128676f,  -0.0018058351f,
  +0.0026144121f,  -0.0000000000f,  -0.0020760219f,  +0.0011387226f,
  +0.0010081104f,  -0.0014409358f,  -0.0000000000f,  +0.0011198434f,
  -0.0006100689f,  -0.0005387764f,  +0.0007729541f,  +0.0000000000f,
  -0.0006218246f,  +0.0003510282f,  +0.0003260878f,  -0.0005000455f,
  -0.0000000000f
};

/**
 * @brief 对ADC采样数据进行时域分析 (包括幅度和频率)
 * @param data_in 指向输入的数据缓冲区 (浮点电压值)
 * @param size 采样点数量
 * @param sample_rate_hz ADC的采样率 (Hz), 用于计算频率
 * @param result_out 指向用于存储分析结果的结构体
 */
void my_time_domain_analysis(const float32_t* data_in, uint32_t size, uint32_t sample_rate_hz, time_domain_result_t* result_out)
{
    if (data_in == NULL || result_out == NULL || size < 2 || sample_rate_hz == 0) { // 至少需要2个点来检测过零
        if (result_out != NULL) {
            memset(result_out, 0, sizeof(time_domain_result_t)); // 清零结果
        }
        return; // 安全检查
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
        float32_t ac_sample = data_in[i] - result_out->dc_offset;
        sum += ac_sample * ac_sample;
    }
    result_out->ac_rms = sqrtf((float32_t)(sum / size));

    // --- 步骤 3: 根据RMS值推算Vpp ---
    result_out->vpp_sine = result_out->ac_rms * 2.0f * 1.41421356f;
    result_out->vpp_square = result_out->ac_rms * 2.0f;


    // =======================================================
    // --- 步骤 4: 新增 - 通过过零检测法计算频率 ---
    // =======================================================
    uint32_t zero_crossings_count = 0; // 记录过零点的数量
    uint32_t first_crossing_index = 0; // 第一个过零点的索引
    uint32_t last_crossing_index = 0;  // 最后一个过零点的索引
    
    // 遍历采样点，寻找从下方穿越直流偏置的点 (上升沿)
    for (uint32_t i = 1; i < size; i++) {
        // 条件：前一个点在直流偏置下方，当前点在直流偏置上方或持平
        if ((data_in[i-1] < result_out->dc_offset) && (data_in[i] >= result_out->dc_offset)) {
            if (zero_crossings_count == 0) {
                first_crossing_index = i; // 记录第一个过零点的位置
            }
            last_crossing_index = i; // 持续更新最后一个过零点的位置
            zero_crossings_count++;
        }
    }

    if (zero_crossings_count >= 2) { // 必须至少检测到2个过零点才能计算周期
        // 计算多个周期的总点数
        uint32_t total_points_for_cycles = last_crossing_index - first_crossing_index;
        // 计算平均每个周期的点数
        float32_t avg_points_per_cycle = (float32_t)total_points_for_cycles / (zero_crossings_count - 1);
        // 计算周期 (T = 点数 / 采样率)
        float32_t period = avg_points_per_cycle / (float32_t)sample_rate_hz;
        // 计算频率 (f = 1 / T)
        result_out->frequency = 1.0f / period;
    } else {
        // 如果没有检测到足够的过零点 (可能信号频率太低或采样时间太短)
        result_out->frequency = 0.0f;
    }
}
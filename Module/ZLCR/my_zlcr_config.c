#include "my_zlcr_config.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "AD9833.h"
#include "AD9954.h"

// 全局频率数组，用于不同扫描场景
// 快速概览场景 (10-20点)
uint32_t g_quick_view_freq_array[20];

// 标准特性表征场景 (50-200点)
uint32_t g_standard_freq_array[200];

// 精细分析场景 (200-1000点)
uint32_t g_fine_analysis_freq_array[1000];

// 谐振点查找场景 (分两步扫描)
uint32_t g_resonant_search_coarse_array[50];  // 宽带粗扫
uint32_t g_resonant_search_fine_array[400];   // 窄带精扫

#define Rx 220 // 模拟前端的电阻是220欧姆

sweep_point_result_t g_current_freq_result;

void my_zlcr_get_impedance(const fundamental_result_t *ch1_fundamental, const fundamental_result_t *ch2_fundamental, 
                           sweep_point_result_t *current_freq_result) {
    // 计算阻抗
    /* (Vch2 / Rx) = (Vch1 / Rz) 借此反推 Rz 得到阻抗 */
    // 计算阻抗 Rz = (Rx * Vch1) / Vch2
    float32_t Vch1 = ch1_fundamental->fundamental_vrms; // ADC1通道的基波有效值
    float32_t Vch2 = ch2_fundamental->fundamental_vrms; // ADC2通道的基波有效值

    current_freq_result->magnitude = (Rx * Vch1) / Vch2;
    current_freq_result->phase = ch1_fundamental->fundamental_phase_angle - ch2_fundamental->fundamental_phase_angle; // 相位差
    current_freq_result->frequency = (ch1_fundamental->fundamental_frequency + ch2_fundamental->fundamental_frequency) / 2.0f; // 频率

}

// 频率点生成函数 - 多十倍频扫描
void generate_decade_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points_per_decade, uint32_t* freq_array, uint32_t* generated_points) {
    // 计算起始和终止频率的对数
    uint32_t start_decade = (uint32_t)floorf(log10f((float)start_freq));
    uint32_t stop_decade = (uint32_t)ceilf(log10f((float)stop_freq));
    
    uint32_t total_points = 0;
    
    // 对每个十倍频程进行扫描
    for (uint32_t decade = start_decade; decade <= stop_decade; decade++) {
        uint32_t decade_start = (uint32_t)powf(10.0f, (float)decade);
        uint32_t decade_stop = decade_start * 10;
        
        // 调整边界
        if (decade_start < start_freq) decade_start = start_freq;
        if (decade_stop > stop_freq) decade_stop = stop_freq;
        
        // 在当前十倍频程内进行对数扫描
        if (decade_stop > decade_start) {
            uint32_t points_in_decade = points_per_decade;
            // 如果是第一个或最后一个十倍频程，可能需要调整点数
            if (decade == start_decade || decade == stop_decade) {
                float ratio = (float)(decade_stop - decade_start) / (float)(decade * 10 - decade_start);
                points_in_decade = (uint32_t)(points_per_decade * ratio);
                if (points_in_decade < 2) points_in_decade = 2;
            }
            
            // 生成当前十倍频程的频率点
            if (total_points + points_in_decade <= *generated_points) {
                generate_logarithmic_frequency_points(decade_start, decade_stop, points_in_decade, &freq_array[total_points]);
                total_points += points_in_decade;
            } else {
                // 如果数组空间不足，只生成部分点
                uint32_t remaining_points = *generated_points - total_points;
                if (remaining_points > 0) {
                    generate_logarithmic_frequency_points(decade_start, decade_stop, remaining_points, &freq_array[total_points]);
                    total_points += remaining_points;
                }
                break;
            }
        }
    }
    
    *generated_points = total_points;
}



// 频率点生成函数 - 线性扫描
void generate_linear_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points, uint32_t* freq_array) {
    if (points <= 1) {
        freq_array[0] = start_freq;
        return;
    }
    
    for (uint32_t i = 0; i < points; i++) {
        freq_array[i] = start_freq + i * (stop_freq - start_freq) / (points - 1);
    }
}

// 频率点生成函数 - 对数扫描
void generate_logarithmic_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points, uint32_t* freq_array) {
    if (points <= 1) {
        freq_array[0] = start_freq;
        return;
    }
    
    if (start_freq == 0) start_freq = 1; // 避免对数计算错误
    
    float log_start = logf((float)start_freq);
    float log_stop = logf((float)stop_freq);
    float log_step = (log_stop - log_start) / (points - 1);
    
    for (uint32_t i = 0; i < points; i++) {
        freq_array[i] = (uint32_t)(expf(log_start + i * log_step) + 0.5f); // 四舍五入
    }
}


#include "my_zlcr_config.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "AD9833.h"
#include "AD9954.h"

// 全局频率数组，用于不同扫描场景
// 快速概览场景 (10-20点)
static uint32_t g_quick_view_freq_array[20];

// 标准特性表征场景 (50-200点)
static uint32_t g_standard_freq_array[200];

// 精细分析场景 (200-1000点)
static uint32_t g_fine_analysis_freq_array[1000];

// 谐振点查找场景 (分两步扫描)
static uint32_t g_resonant_search_coarse_array[50];  // 宽带粗扫
static uint32_t g_resonant_search_fine_array[400];   // 窄带精扫


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

// 频率点生成函数 - 根据配置生成频率点
void generate_frequency_points(sweep_config_t* config) {
    // 根据扫描策略选择合适的频率数组
    switch (config->strategy) {
        case STRATEGY_QUICK_VIEW:
            config->freq_array = g_quick_view_freq_array;
            config->array_length = sizeof(g_quick_view_freq_array) / sizeof(g_quick_view_freq_array[0]);
            break;
            
        case STRATEGY_STANDARD:
            config->freq_array = g_standard_freq_array;
            config->array_length = sizeof(g_standard_freq_array) / sizeof(g_standard_freq_array[0]);
            break;
            
        case STRATEGY_FINE_ANALYSIS:
            config->freq_array = g_fine_analysis_freq_array;
            config->array_length = sizeof(g_fine_analysis_freq_array) / sizeof(g_fine_analysis_freq_array[0]);
            break;
            
        case STRATEGY_RESONANT_SEARCH:
            // 对于谐振点查找，我们使用粗扫数组
            config->freq_array = g_resonant_search_coarse_array;
            config->array_length = sizeof(g_resonant_search_coarse_array) / sizeof(g_resonant_search_coarse_array[0]);
            break;
            
        default:
            // 默认使用标准数组
            config->freq_array = g_standard_freq_array;
            config->array_length = sizeof(g_standard_freq_array) / sizeof(g_standard_freq_array[0]);
            break;
    }
    
    // 确保数组长度不小于配置的点数
    if (config->array_length < config->points) {
        config->points = config->array_length;
    }
    
    // 根据扫描类型生成频率点
    if (config->type == SWEEP_LINEAR) {
        generate_linear_frequency_points(config->start_freq, config->stop_freq, config->points, config->freq_array);
    } else {
        generate_logarithmic_frequency_points(config->start_freq, config->stop_freq, config->points, config->freq_array);
    }
}
// 根据扫描策略设置默认点数
void set_default_points_by_strategy(sweep_config_t* config) {
    switch (config->strategy) {
        case STRATEGY_QUICK_VIEW:
            config->points = 20;  // 快速概览使用20个点
            break;
            
        case STRATEGY_STANDARD:
            config->points = 100; // 标准特性表征使用100个点
            break;
            
        case STRATEGY_FINE_ANALYSIS:
            config->points = 500; // 精细分析使用500个点
            break;
            
        case STRATEGY_RESONANT_SEARCH:
            config->points = 50;  // 谐振点查找粗扫使用50个点
            break;
            
        default:
            config->points = 100; // 默认使用100个点
            break;
    }
    
    // 确保点数不超过数组容量
    if (config->strategy == STRATEGY_QUICK_VIEW && config->points > 20) {
        config->points = 20;
    } else if (config->strategy == STRATEGY_STANDARD && config->points > 201) {
        config->points = 201;
    } else if (config->strategy == STRATEGY_FINE_ANALYSIS && config->points > 1000) {
        config->points = 1000;
    } else if (config->strategy == STRATEGY_RESONANT_SEARCH && config->points > 50) {
        config->points = 50;
    }
}

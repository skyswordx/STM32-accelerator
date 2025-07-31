#include "my_time_detect.h"
#include <string.h>
#include <math.h>
#include <stddef.h> // For NULL

/**
 * @brief 对ADC采样数据进行优化的时域分析
 */
void my_time_domain_analysis_optimized(const float32_t* data_in, 
                                       float32_t* temp_buffer, 
                                       uint32_t size, 
                                       uint32_t sample_rate_hz, 
                                       const time_domain_config_t* config, 
                                       time_domain_result_t* result_out)
{
    if (data_in == NULL || temp_buffer == NULL || config == NULL || result_out == NULL || size < 10 || sample_rate_hz == 0) {
        if (result_out != NULL) {
            memset(result_out, 0, sizeof(time_domain_result_t));
        }
        return; // 安全检查
    }

    const float32_t* processing_data; // 指向将要被处理的数据

    // --- 步骤 1: (可选) 低通滤波 ---
    if (config->enable_filter) {
        // 使用传入的 temp_buffer 作为滤波输出缓冲区
        temp_buffer[0] = data_in[0]; // 初始化第一个值
        for (uint32_t i = 1; i < size; i++) {
            temp_buffer[i] = config->filter_alpha * data_in[i] + (1.0f - config->filter_alpha) * temp_buffer[i-1];
        }
        processing_data = temp_buffer; // 后续处理使用滤波后的数据
    } else {
        processing_data = data_in; // 直接使用原始数据
    }

    // --- 步骤 2: 计算直流分量和峰-峰值 ---
    float64_t sum = 0.0;
    result_out->v_max = processing_data[0];
    result_out->v_min = processing_data[0];

    for (uint32_t i = 0; i < size; i++) {
        sum += processing_data[i];
        if (processing_data[i] > result_out->v_max) result_out->v_max = processing_data[i];
        if (processing_data[i] < result_out->v_min) result_out->v_min = processing_data[i];
    }
    result_out->dc_offset = (float32_t)(sum / size);
    result_out->vpp_peak = result_out->v_max - result_out->v_min;

    // --- 步骤 3: 计算RMS值 ---
    sum = 0.0;
    for (uint32_t i = 0; i < size; i++) {
        float32_t ac_sample = processing_data[i] - result_out->dc_offset;
        sum += ac_sample * ac_sample;
    }
    result_out->ac_rms = sqrtf((float32_t)(sum / size));

    // --- 步骤 4: 频率测量 (迟滞比较 + 线性插值) ---
    float32_t v_high = result_out->dc_offset + config->hysteresis_v;
    float32_t v_low  = result_out->dc_offset - config->hysteresis_v;
    uint8_t state = (processing_data[0] > v_high) ? 1 : 0; // 初始状态: 1=高, 0=低
    
    uint32_t crossings_count = 0;
    // 重用 temp_buffer 来存储精确的过零点时刻 (以采样点为单位)
    float32_t* crossing_points = temp_buffer; 

    for (uint32_t i = 1; i < size; i++) {
        if (state == 0 && processing_data[i] > v_high) { // 从低到高穿越
            // 线性插值计算精确过零点
            float32_t v1 = processing_data[i-1] - result_out->dc_offset;
            float32_t v2 = processing_data[i] - result_out->dc_offset;
            float32_t fraction = fabsf(v1) / (fabsf(v1) + fabsf(v2));
            crossing_points[crossings_count++] = (float32_t)(i - 1) + fraction;
            state = 1;
        } else if (state == 1 && processing_data[i] < v_low) { // 从高到低穿越
            state = 0;
        }
    }

    if (crossings_count >= 2) {
        float32_t total_points_for_cycles = crossing_points[crossings_count - 1] - crossing_points[0];
        float32_t avg_points_per_cycle = total_points_for_cycles / (float32_t)(crossings_count - 1);
        float32_t period = avg_points_per_cycle / (float32_t)sample_rate_hz;
        result_out->frequency = 1.0f / period;
    } else {
        result_out->frequency = 0.0f;
    }
}



/**
 * @brief [新增] 提取整数个周期并重采样到指定长度 (用于FFT前处理)
 */
int my_resample_integer_cycles(const float32_t* data_in, 
                               uint32_t in_size, 
                               float32_t* data_out, 
                               uint32_t out_size,
                               const time_domain_config_t* config)
{
    if (data_in == NULL || data_out == NULL || config == NULL || in_size < 10 || out_size == 0) {
        return 0; // 安全检查
    }

    // --- 步骤 1: 计算直流分量 ---
    float64_t sum = 0.0;
    for (uint32_t i = 0; i < in_size; i++) {
        sum += data_in[i];
    }
    float32_t dc_offset = (float32_t)(sum / in_size);

    // --- 步骤 2: 使用迟滞比较法找到所有上升沿过零点 ---
    float32_t v_high = dc_offset + config->hysteresis_v;
    float32_t v_low  = dc_offset - config->hysteresis_v;
    uint8_t state = (data_in[0] > v_high) ? 1 : 0;
    
    uint32_t crossings_count = 0;
    // 临时使用输出缓冲区的前半部分来存储过零点索引，避免额外分配内存
    uint32_t* crossing_indices = (uint32_t*)data_out; 

    for (uint32_t i = 1; i < in_size; i++) {
        if (state == 0 && data_in[i] > v_high) {
            crossing_indices[crossings_count++] = i;
            state = 1;
            // 检查临时缓冲区是否会溢出 (不太可能发生，但作为安全措施)
            if (crossings_count >= out_size / 2) break;
        } else if (state == 1 && data_in[i] < v_low) {
            state = 0;
        }
    }

    if (crossings_count < 2) {
        return 0; // 未找到至少一个完整周期，无法处理
    }

    // --- 步骤 3: 确定用于重采样的整数周期的起始和结束点 ---
    uint32_t start_index = crossing_indices[0];
    uint32_t end_index = crossing_indices[crossings_count - 1];
    uint32_t source_len = end_index - start_index;

    if (source_len == 0) {
        return 0;
    }
    
    const float32_t* source_data = &data_in[start_index];

    // --- 步骤 4: 线性插值重采样 ---
    // y = y1 + (y2-y1) * (x - x1) / (x2-x1)
    for (uint32_t i = 0; i < out_size; i++) {
        float32_t src_pos = (float32_t)i * (float32_t)(source_len - 1) / (float32_t)(out_size - 1);
        uint32_t index = (uint32_t)floorf(src_pos);
        float32_t fraction = src_pos - (float32_t)index;

        // 防止索引越界
        if (index >= source_len - 1) {
            index = source_len - 2;
        }

        float32_t y1 = source_data[index];
        float32_t y2 = source_data[index + 1];
        
        data_out[i] = y1 + (y2 - y1) * fraction;
    }

    return 1; // 成功
}

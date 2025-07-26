#include "my_quinn_config.h"

// --- 全局变量 ---
static quinn_frequency_result_t g_quinn_result; // Quinn算法结果结构体
uint8_t g_quinn_module_enabled = 1; // Quinn模块运行时启用标志，默认启用

extern uint32_t g_ADC_SAMPLE_RATE_Hz; // 从ADC模块获取采样率

/**
 * @brief 执行Quinn频率估计算法
 * @details 基于FFT结果应用Quinn算法进行精细频率估计
 * @param[in] fft_data FFT复数结果数组
 * @param[in] peak_index 检测到的峰值点索引
 * @param[out] result 频率估计结果结构体指针
 * @return 0表示成功，其他值表示失败
 */
int my_quinn_process(const float32_t* fft_data, uint16_t peak_index, quinn_frequency_result_t* result)
{
    // 如果模块被禁用，直接返回错误
    if (!g_quinn_module_enabled) {
        return MY_QUINN_ERROR_PROCESS_FAILED;
    }
    
    // 检查输入参数
    if (fft_data == NULL || result == NULL) {
        return MY_QUINN_ERROR_INVALID_PARAMETER;
    }
    
    // 检查峰值索引有效性
    if (peak_index == 0 || peak_index >= (QUINN_FFT_LENGTH / 2 - 1)) {
        return MY_QUINN_ERROR_INVALID_PEAK_INDEX;
    }
    
    // 保存峰值索引
    g_quinn_result.peak_index = peak_index;
    
    // 计算粗略频率估计
    g_quinn_result.coarse_frequency = (float32_t)peak_index * g_ADC_SAMPLE_RATE_Hz / QUINN_FFT_LENGTH;
    
    // 获取复数FFT值
    float32_t real_k_minus_1 = fft_data[2 * (peak_index - 1)];
    float32_t imag_k_minus_1 = fft_data[2 * (peak_index - 1) + 1];
    float32_t real_k = fft_data[2 * peak_index];
    float32_t imag_k = fft_data[2 * peak_index + 1];
    float32_t real_k_plus_1 = fft_data[2 * (peak_index + 1)];
    float32_t imag_k_plus_1 = fft_data[2 * (peak_index + 1) + 1];
    
    // 计算复数除法: X[k-1]/X[k] 和 X[k+1]/X[k]
    // 复数除法: (a+jb)/(c+jd) = [(a+jb)(c-jd)] / [c^2+d^2]
    float32_t denominator_k = real_k * real_k + imag_k * imag_k;
    
    // 避免除零错误
    if (denominator_k == 0.0f) {
        g_quinn_result.validity = 0;
        g_quinn_result.confidence = 0.0f;
        *result = g_quinn_result;
        return MY_QUINN_ERROR_PROCESS_FAILED;
    }
    
    // 计算 beta1 = Re{X[k-1] / X[k]}
    float32_t real_div_k_minus_1 = (real_k_minus_1 * real_k + imag_k_minus_1 * imag_k) / denominator_k;
    float32_t imag_div_k_minus_1 = (imag_k_minus_1 * real_k - real_k_minus_1 * imag_k) / denominator_k;
    float32_t beta_1 = real_div_k_minus_1;
    
    // 计算 beta2 = Re{X[k+1] / X[k]}
    float32_t real_div_k_plus_1 = (real_k_plus_1 * real_k + imag_k_plus_1 * imag_k) / denominator_k;
    float32_t imag_div_k_plus_1 = (imag_k_plus_1 * real_k - real_k_plus_1 * imag_k) / denominator_k;
    float32_t beta_2 = real_div_k_plus_1;
    
    // 计算 delta1 和 delta2
    float32_t delta_1, delta_2;
    
    // 避免除零错误
    if ((1.0f - beta_1) == 0.0f) {
        delta_1 = 0.0f;
    } else {
        delta_1 = beta_1 / (1.0f - beta_1);
    }
    
    // 避免除零错误
    if ((beta_2 - 1.0f) == 0.0f) {
        delta_2 = 0.0f;
    } else {
        delta_2 = beta_2 / (beta_2 - 1.0f);
    }
    
    // 选择最终的delta值
    float32_t delta;
    if (delta_1 > 0 && delta_2 > 0) {
        delta = delta_2;
    } else {
        delta = delta_1;
    }
    
    // 保存delta值
    g_quinn_result.delta = delta;
    
    // 计算精确频率
    g_quinn_result.frequency = (peak_index + delta) * g_ADC_SAMPLE_RATE_Hz / QUINN_FFT_LENGTH;
    
    // 计算可信度（基于delta的大小）
    // delta越接近0，结果越可信
    float32_t abs_delta = fabsf(delta);
    if (abs_delta <= 0.01f) {
        g_quinn_result.confidence = 1.0f;
    } else if (abs_delta <= 0.1f) {
        g_quinn_result.confidence = 0.9f - (abs_delta - 0.01f) * 8.0f;
    } else if (abs_delta <= 0.5f) {
        g_quinn_result.confidence = 0.2f - (abs_delta - 0.1f) * 0.4f;
    } else {
        g_quinn_result.confidence = 0.0f;
    }
    
    // 确保可信度在合理范围内
    if (g_quinn_result.confidence < 0.0f) {
        g_quinn_result.confidence = 0.0f;
    }
    
    // 检查结果有效性
    if (g_quinn_result.confidence >= QUINN_CONFIDENCE_THRESHOLD) {
        g_quinn_result.validity = 1;
    } else {
        g_quinn_result.validity = 0;
    }
    
    // 返回结果
    *result = g_quinn_result;
    
    return MY_QUINN_SUCCESS;
}

/**
 * @brief 获取Quinn算法的估计结果
 * @param[out] result 频率估计结果结构体指针
 */
void my_quinn_get_result(quinn_frequency_result_t* result)
{
    if (result != NULL) {
        *result = g_quinn_result;
    }
}

/**
 * @brief 检查Quinn算法结果是否有效
 * @return 1表示有效，0表示无效
 */
uint8_t my_quinn_is_result_valid(void)
{
    return g_quinn_result.validity;
}

/**
 * @brief 获取Quinn算法结果的可信度
 * @return 可信度值 (0.0-1.0)
 */
float32_t my_quinn_get_confidence(void)
{
    return g_quinn_result.confidence;
}
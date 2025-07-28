#include "my_dac_config.h"
#include "my_parameter_config.h"  // 包含参数配置头文件

// 定义波形缓冲区
uint16_t* g_dac_waveform_buffer;

uint16_t g_dac_sine[256] = {
    2048, 2098, 2148, 2198, 2248, 2298, 2348, 2398, 2447, 2496, 2545, 2594, 2642, 2690, 2737, 2784,
    2831, 2877, 2923, 2968, 3013, 3057, 3100, 3143, 3185, 3226, 3267, 3307, 3346, 3385, 3423, 3459,
    3495, 3530, 3565, 3598, 3630, 3662, 3692, 3722, 3750, 3777, 3804, 3829, 3853, 3876, 3898, 3919,
    3939, 3958, 3976, 3992, 4007, 4021, 4034, 4045, 4056, 4065, 4073, 4080, 4085, 4089, 4093, 4095,
    4095, 4095, 4093, 4089, 4085, 4080, 4073, 4065, 4056, 4045, 4034, 4021, 4007, 3992, 3976, 3958,
    3939, 3919, 3898, 3876, 3853, 3829, 3804, 3777, 3750, 3722, 3692, 3662, 3630, 3598, 3565, 3530,
    3495, 3459, 3423, 3385, 3346, 3307, 3267, 3226, 3185, 3143, 3100, 3057, 3013, 2968, 2923, 2877,
    2831, 2784, 2737, 2690, 2642, 2594, 2545, 2496, 2447, 2398, 2348, 2298, 2248, 2198, 2148, 2098,
    2048, 1998, 1948, 1898, 1848, 1798, 1748, 1698, 1649, 1600, 1551, 1502, 1454, 1406, 1359, 1312,
    1265, 1219, 1173, 1128, 1083, 1039,  996,  953,  911,  870,  829,  789,  750,  711,  673,  637,
     601,  566,  531,  498,  466,  434,  404,  374,  346,  319,  292,  267,  243,  220,  198,  177,
     157,  138,  120,  104,   89,   75,   62,   51,   40,   31,   23,   16,   11,    7,    3,    1,
       1,    1,    3,    7,   11,   16,   23,   31,   40,   51,   62,   75,   89,  104,  120,  138,
     157,  177,  198,  220,  243,  267,  292,  319,  346,  374,  404,  434,  466,  498,  531,  566,
     601,  637,  673,  711,  750,  789,  829,  870,  911,  953,  996, 1039, 1083, 1128, 1173, 1219,
    1265, 1312, 1359, 1406, 1454, 1502, 1551, 1600, 1649, 1698, 1748, 1798, 1848, 1898, 1948, 1998
};

uint16_t g_dac_cosine[256] = {
    4095, 4095, 4093, 4089, 4085, 4080, 4073, 4065, 4056, 4045, 4034, 4021, 4007, 3992, 3976, 3958,
    3939, 3919, 3898, 3876, 3853, 3829, 3804, 3777, 3750, 3722, 3692, 3662, 3630, 3598, 3565, 3530,
    3495, 3459, 3423, 3385, 3346, 3307, 3267, 3226, 3185, 3143, 3100, 3057, 3013, 2968, 2923, 2877,
    2831, 2784, 2737, 2690, 2642, 2594, 2545, 2496, 2447, 2398, 2348, 2298, 2248, 2198, 2148, 2098,
    2048, 1998, 1948, 1898, 1848, 1798, 1748, 1698, 1649, 1600, 1551, 1502, 1454, 1406, 1359, 1312,
    1265, 1219, 1173, 1128, 1083, 1039,  996,  953,  911,  870,  829,  789,  750,  711,  673,  637,
     601,  566,  531,  498,  466,  434,  404,  374,  346,  319,  292,  267,  243,  220,  198,  177,
     157,  138,  120,  104,   89,   75,   62,   51,   40,   31,   23,   16,   11,    7,    3,    1,
       1,    1,    3,    7,   11,   16,   23,   31,   40,   51,   62,   75,   89,  104,  120,  138,
     157,  177,  198,  220,  243,  267,  292,  319,  346,  374,  404,  434,  466,  498,  531,  566,
     601,  637,  673,  711,  750,  789,  829,  870,  911,  953,  996, 1039, 1083, 1128, 1173, 1219,
    1265, 1312, 1359, 1406, 1454, 1502, 1551, 1600, 1649, 1698, 1748, 1798, 1848, 1898, 1948, 1998,
    2048, 2098, 2148, 2198, 2248, 2298, 2348, 2398, 2447, 2496, 2545, 2594, 2642, 2690, 2737, 2784,
    2831, 2877, 2923, 2968, 3013, 3057, 3100, 3143, 3185, 3226, 3267, 3307, 3346, 3385, 3423, 3459,
    3495, 3530, 3565, 3598, 3630, 3662, 3692, 3722, 3750, 3777, 3804, 3829, 3853, 3876, 3898, 3919,
    3939, 3958, 3976, 3992, 4007, 4021, 4034, 4045, 4056, 4065, 4073, 4080, 4085, 4089, 4093, 4095
};

uint16_t g_dac_square[256] = {
0
};

uint16_t g_dac_triangle[256] = {
0
};


/**
 * @brief 根据全局参数更新DAC波形
 * @note  此函数根据全局参数生成相应的波形数据并存储在g_dac_waveform_buffer中
 */
void update_dac_waveform_by_parameters(void)
{
    switch (g_desired_DAC_output_waveform) {
        case 0: // 正弦波
            g_dac_waveform_buffer = g_dac_sine;
            break;
        case 1: // 方波
            g_dac_waveform_buffer = g_dac_square;
            break;
        case 2: // 三角波
            g_dac_waveform_buffer = g_dac_triangle;
            break;
        default:
            // 默认生成正弦波
            g_dac_waveform_buffer = g_dac_sine;
            break;
    }
}

/**
 * @brief 生成正弦波数据
 * @param buffer 数据缓冲区
 * @param size 缓冲区大小
 * @param frequency 频率(Hz)
 * @param amplitude 幅度(V)
 * @param vref 参考电压(V)
 */
void generate_sine_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref)
{
    float32_t step = 2.0f * PI / size;
    float32_t scale = amplitude / vref * DAC_MAX_VALUE;
    
    for (uint32_t i = 0; i < size; i++) {
        float32_t value = scale * arm_sin_f32(i * step) + DAC_MAX_VALUE / 2;
        // 确保值在有效范围内
        if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
        if (value < 0) value = 0;
        buffer[i] = (uint16_t)value;
    }
}

/**
 * @brief 生成方波数据
 * @param buffer 数据缓冲区
 * @param size 缓冲区大小
 * @param frequency 频率(Hz)
 * @param amplitude 幅度(V)
 * @param vref 参考电压(V)
 */
void generate_square_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref)
{
    float32_t scale = amplitude / vref * DAC_MAX_VALUE;
    uint32_t half_size = size / 2;
    
    for (uint32_t i = 0; i < size; i++) {
        float32_t value;
        if (i < half_size) {
            value = scale + DAC_MAX_VALUE / 2;
        } else {
            value = -scale + DAC_MAX_VALUE / 2;
        }
        // 确保值在有效范围内
        if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
        if (value < 0) value = 0;
        buffer[i] = (uint16_t)value;
    }
}

/**
 * @brief 生成三角波数据
 * @param buffer 数据缓冲区
 * @param size 缓冲区大小
 * @param frequency 频率(Hz)
 * @param amplitude 幅度(V)
 * @param vref 参考电压(V)
 */
void generate_triangle_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref)
{
    float32_t scale = amplitude / vref * DAC_MAX_VALUE;
    uint32_t half_size = size / 2;
    
    for (uint32_t i = 0; i < size; i++) {
        float32_t value;
        if (i < half_size) {
            value = (2.0f * scale * i / half_size) - scale + DAC_MAX_VALUE / 2;
        } else {
            value = (-2.0f * scale * (i - half_size) / half_size) + scale + DAC_MAX_VALUE / 2;
        }
        // 确保值在有效范围内
        if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
        if (value < 0) value = 0;
        buffer[i] = (uint16_t)value;
    }
}

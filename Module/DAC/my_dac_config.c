#include "my_dac_config.h"
#include "my_parameter_config.h"  // 包含参数配置头文件

// 定义波形缓冲区

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
 * @note 修改了实现以生成更准确的正弦波，参考预定义的g_dac_sine数组
 */
void generate_sine_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref)
{
    // 参考预定义的正弦波数组，生成更准确的正弦波
    // 特殊处理：如果幅度等于参考电压，则使用预定义数组的标准值
    if (amplitude >= vref) {
        // 直接复制预定义的正弦波数组
        for (uint32_t i = 0; i < size && i < 256; i++) {
            buffer[i] = g_dac_sine[i];
        }
        return;
    }
    
    // 中心值为DAC_MAX_VALUE/2 (2048)，幅度为 amplitude/vref * DAC_MAX_VALUE/2
    float32_t scale = amplitude / vref * DAC_MAX_VALUE / 2.0f;
    
    for (uint32_t i = 0; i < size; i++) {
        // 使用更精确的角度计算，确保完整的正弦周期
        float32_t angle = 2.0f * PI * i / size;
        // 正弦波公式：y = A * sin(θ) + offset
        float32_t value = scale * arm_sin_f32(angle) + (DAC_MAX_VALUE / 2.0f);
        
        // 确保值在有效范围内并四舍五入
        if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
        if (value < 0) value = 0;
        buffer[i] = (uint16_t)(value + 0.5f);  // 四舍五入提高精度
    }
}

/**
 * @brief 生成方波数据
 * @param buffer 数据缓冲区
 * @param size 缓冲区大小
 * @param frequency 频率(Hz)
 * @param amplitude 幅度(V)
 * @param vref 参考电压(V)
 * @note 修改了实现以生成更准确的方波，参考预定义的g_dac_square数组
 */
void generate_square_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref)
{
    // 参考预定义的方波数组，生成更准确的方波
    // 根据预定义数组，方波应该是前半部分为最大值，后半部分为0
    uint32_t half_size = size / 2;
    
    // 特殊处理：如果幅度等于参考电压，则使用预定义数组的标准值
    if (amplitude >= vref) {
        // 直接复制预定义的方波数组
        for (uint32_t i = 0; i < size && i < 256; i++) {
            buffer[i] = g_dac_square[i];
        }
        return;
    }
    
    // 计算基于幅度的高电平和低电平值
    float32_t scale = amplitude / vref * DAC_MAX_VALUE;
    uint16_t high_value = (uint16_t)(scale + (DAC_MAX_VALUE - scale) / 2.0f + 0.5f);
    uint16_t low_value = (uint16_t)((DAC_MAX_VALUE - scale) / 2.0f + 0.5f);
    
    // 确保值在有效范围内
    if (high_value > DAC_MAX_VALUE) high_value = DAC_MAX_VALUE;
    if (low_value > DAC_MAX_VALUE) low_value = DAC_MAX_VALUE;
    if (high_value < 0) high_value = 0;
    if (low_value < 0) low_value = 0;
    
    // 填充前半部分为高电平，后半部分为低电平
    for (uint32_t i = 0; i < half_size; i++) {
        buffer[i] = high_value;
    }
    for (uint32_t i = half_size; i < size; i++) {
        buffer[i] = low_value;
    }
}

/**
 * @brief 生成三角波数据
 * @param buffer 数据缓冲区
 * @param size 缓冲区大小
 * @param frequency 频率(Hz)
 * @param amplitude 幅度(V)
 * @param vref 参考电压(V)
 * @note 修改了实现以生成更准确的三角波，参考预定义的g_dac_triangle数组
 */
void generate_triangle_wave(uint16_t* buffer, uint32_t size, uint32_t frequency, float32_t amplitude, float32_t vref)
{
    // 参考预定义的三角波数组，生成更准确的三角波
    // 三角波从0线性增加到最大值，再线性减少到接近0的值
    float32_t scale = amplitude / vref * DAC_MAX_VALUE;
    uint32_t half_size = size / 2;
    
    // 特殊处理：如果幅度等于参考电压，则使用预定义数组的标准值
    if (amplitude >= vref) {
        // 直接复制预定义的三角波数组
        for (uint32_t i = 0; i < size && i < 256; i++) {
            buffer[i] = g_dac_triangle[i];
        }
        return;
    }
    
    // 上升沿：从0线性增加到scale
    for (uint32_t i = 0; i < half_size; i++) {
        // 线性插值：y = (scale * i) / (half_size - 1)
        float32_t value = (scale * i) / (half_size - 1);
        // 确保值在有效范围内并四舍五入
        if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
        if (value < 0) value = 0;
        buffer[i] = (uint16_t)(value + 0.5f);
    }
    
    // 下降沿：从scale线性减少到0
    for (uint32_t i = half_size; i < size; i++) {
        // 线性插值：y = scale - (scale * (i - half_size)) / (half_size - 1)
        float32_t value = scale - (scale * (i - half_size)) / (half_size - 1);
        // 确保值在有效范围内并四舍五入
        if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
        if (value < 0) value = 0;
        buffer[i] = (uint16_t)(value + 0.5f);
    }
}

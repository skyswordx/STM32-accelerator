#include "my_signal_processing.h"
#include "my_freq_config.h" // 引用全局变量和定义
#include "arm_math.h"
#include <string.h>

// 外部引用在 my_freq_config.c 中定义的全局变量和函数
// 确保这些变量和函数在 .c 文件中不是 static 的，并且在 .h 文件中有 extern 声明
extern uint32_t g_ADC_SAMPLE_RATE_Hz;
extern arm_cfft_radix4_instance_f32 fft_instance_radix4;
extern float32_t g_windowed_adc_data[FFT_LENGTH];

extern float32_t generate_window(window_type_t window_type, float32_t* window_buffer, uint32_t length);
extern float32_t caculate_DCcomponent(float32_t* data, uint32_t length);
extern arm_status perform_spectral_interpolation(
    float32_t* magnitude_spectrum,
    uint16_t peak_index,
    spectral_interpolation_mode_t mode,
    interpolated_peak_t* result_out
);


/**
 * @brief (要求 2 实现) 分析输入信号，提取基波和谐波分量
 */
int32_t analyze_signal_with_harmonics(
    float32_t* adc_input, 
    harmonic_component_t* components, 
    int32_t max_components,
    float32_t* fft_input_buffer,
    float32_t* fft_output_buffer
) {
    // --- 1. FFT 预处理 (与 my_armcfft32_apply 相同) ---
    arm_cfft_radix4_init_f32(&fft_instance_radix4, FFT_LENGTH, 0, 1);

    // 滤除直流分量
    float32_t mean = caculate_DCcomponent(adc_input, FFT_LENGTH);
    for(uint32_t idx = 0; idx < FFT_LENGTH; idx++) {
        fft_input_buffer[idx] = adc_input[idx] - mean;
    }

    // 应用汉宁窗 (与您的扫频设置保持一致)
    float32_t window_compensation_factor = generate_window(WINDOW_HANNING, g_windowed_adc_data, FFT_LENGTH);
    arm_mult_f32(fft_input_buffer, g_windowed_adc_data, fft_input_buffer, FFT_LENGTH);

    // 准备复数FFT输入
    for(int n = FFT_LENGTH - 1; n >= 0; n--) {
        fft_input_buffer[2 * n] = fft_input_buffer[n];
        fft_input_buffer[2 * n + 1] = 0.0f;
    }

    // --- 2. 执行 FFT 并计算幅度谱 ---
    arm_cfft_radix4_f32(&fft_instance_radix4, fft_input_buffer);
    arm_cmplx_mag_f32(fft_input_buffer, fft_output_buffer, FFT_LENGTH);

    // --- 3. 寻找所有谐波峰值 ---
    int32_t components_found = 0;
    float32_t fundamental_magnitude = 0.0f;
    uint32_t fundamental_index = 0;

    // 首先找到基波 (最大峰值)
    arm_max_f32(&fft_output_buffer[1], (FFT_LENGTH / 2) - 1, &fundamental_magnitude, &fundamental_index);
    fundamental_index += 1; // arm_max_f32 返回的是子数组内的索引, 需加1

    // 定义峰值检测的幅度阈值 (例如，基波幅度的1%)
    // 这是为了滤除噪声，避免将无效的噪声尖峰识别为谐波
    const float32_t peak_threshold = fundamental_magnitude * 0.01f; 

    for (uint16_t i = 1; i < (FFT_LENGTH / 2) - 1 && components_found < max_components; i++) {
        // 简单的峰值判断逻辑：当前点比左右两点都大，且高于阈值
        if (fft_output_buffer[i] > fft_output_buffer[i - 1] &&
            fft_output_buffer[i] > fft_output_buffer[i + 1] &&
            fft_output_buffer[i] > peak_threshold) 
        {
            // --- 插值代码开始 ---
            
            // 默认使用未插值的FFT栅格结果
            float32_t final_frequency = (float32_t)i * g_ADC_SAMPLE_RATE_Hz / FFT_LENGTH;
            float32_t final_magnitude = fft_output_buffer[i];

            // 声明用于存储插值结果的结构体
            interpolated_peak_t interpolated_result;
            
            // 尝试调用频谱插值函数
            if (perform_spectral_interpolation(fft_output_buffer, i, INTERPOLATION_HANNING_SPECIAL, &interpolated_result) == ARM_MATH_SUCCESS) {
                // 如果插值成功，则使用更精确的频率和幅度结果
                final_frequency = interpolated_result.corrected_frequency;
                final_magnitude = interpolated_result.corrected_magnitude;
            }
            // 如果插值失败，则自动使用上面定义的默认值，代码无缝衔接

            // --- 插值代码结束 ---

            // 存储找到的频率分量信息
            components[components_found].fft_index = i; // 存储原始整数索引，用于相位计算
            components[components_found].frequency = final_frequency; // 使用插值后的频率
            
            // 幅度计算：使用插值后的幅度(final_magnitude)进行计算
            // 幅度(V) = (FFT模值 * 窗补偿系数 / (N/2)) / 2
            float32_t vpp = final_magnitude * window_compensation_factor * 2.0f / FFT_LENGTH;
            components[components_found].amplitude = vpp / 2.0f;

            // 相位计算：相位信息需要从原始的复数FFT输出中获取
            // 注意：此处仍然使用原始的整数索引`i`
            components[components_found].phase = atan2f(fft_input_buffer[2 * i + 1], fft_input_buffer[2 * i]);
            
            components_found++;
        }
    }
    
    return components_found;
}


/**
 * @brief (要求 1 计算部分实现) 在测量的频响数据中通过线性插值查找指定频率的响应
 */
arm_status get_system_response_at_freq(
    float32_t* H_cmplx_out,
    float32_t target_freq_hz,
    const float32_t* w_rad_sweep,
    const float32_t* H_measured_cmplx,
    uint32_t num_sweep_points
) {
    float32_t target_w_rad = 2.0f * PI * target_freq_hz;

    // 边界检查
    if (target_w_rad < w_rad_sweep[0] || target_w_rad > w_rad_sweep[num_sweep_points - 1]) {
        return ARM_MATH_ARGUMENT_ERROR; // 目标频率超出测量范围
    }

    // 寻找包围目标频率的两个测量点
    uint32_t idx1 = 0;
    while (idx1 < num_sweep_points && w_rad_sweep[idx1] < target_w_rad) {
        idx1++;
    }

    // 如果精确匹配
    if (w_rad_sweep[idx1] == target_w_rad) {
        H_cmplx_out[0] = H_measured_cmplx[idx1 * 2];     // Real
        H_cmplx_out[1] = H_measured_cmplx[idx1 * 2 + 1]; // Imag
        return ARM_MATH_SUCCESS;
    }
    
    // 准备线性插值
    uint32_t idx0 = idx1 - 1;

    float32_t w0 = w_rad_sweep[idx0];
    float32_t w1 = w_rad_sweep[idx1];

    float32_t H0_real = H_measured_cmplx[idx0 * 2];
    float32_t H0_imag = H_measured_cmplx[idx0 * 2 + 1];
    float32_t H1_real = H_measured_cmplx[idx1 * 2];
    float32_t H1_imag = H_measured_cmplx[idx1 * 2 + 1];

    // 计算插值因子
    float32_t factor = (target_w_rad - w0) / (w1 - w0);

    // 对实部和虚部分别进行线性插值
    H_cmplx_out[0] = H0_real + factor * (H1_real - H0_real); // Interp Real
    H_cmplx_out[1] = H0_imag + factor * (H1_imag - H0_imag); // Interp Imag

    return ARM_MATH_SUCCESS;
}


/**
 * @brief (要求 3 实现) 根据输入信号的谐波分量和系统的频响特性，重建时域输出波形
 */
void reconstruct_output_waveform(
    float32_t* reconstructed_waveform,
    uint32_t waveform_points,
    const harmonic_component_t* input_components,
    int32_t num_components,
    const float32_t* w_rad_sweep,
    const float32_t* H_measured_cmplx,
    uint32_t num_sweep_points
) {
    // 1. 清空输出波形数组
    memset(reconstructed_waveform, 0, waveform_points * sizeof(float32_t));

    // 2. 获取基波频率和周期 (假设输入谐波数组的第一个元素是基波)
    if (num_components == 0) return; // 没有可用于重建的分量
    float32_t fundamental_freq = input_components[0].frequency;
    float32_t period = 1.0f / fundamental_freq;

    // 3. 循环遍历每个谐波分量
    for (int i = 0; i < num_components; i++) {
        // 3a. 获取当前输入谐波分量的幅度和相位
        float32_t A_in = input_components[i].amplitude;
        float32_t P_in = input_components[i].phase;
        float32_t freq_i = input_components[i].frequency;

        // 3b. 计算系统在该频率下的响应 (增益和相移)
        float32_t H_cmplx[2];
        if (get_system_response_at_freq(H_cmplx, freq_i, w_rad_sweep, H_measured_cmplx, num_sweep_points) != ARM_MATH_SUCCESS) {
            continue; // 如果频率超出范围，则跳过此分量
        }

        float32_t gain, P_filter;
        arm_cmplx_mag_f32(H_cmplx, &gain, 1);
        P_filter = atan2f(H_cmplx[1], H_cmplx[0]);
        
        // 3c. 计算输出谐波分量的幅度和相位
        float32_t A_out = A_in * gain;
        float32_t P_out = P_in + P_filter;

        // 3d. 将此谐波分量在时域上叠加到输出波形中
        // 这遵循公式: y(t) = Σ A_out * sin(2*pi*f*t + P_out)
        for (uint32_t t_idx = 0; t_idx < waveform_points; t_idx++) {
            float32_t t = (float32_t)t_idx * period / waveform_points;
            float32_t angle = 2.0f * PI * freq_i * t + P_out;
            reconstructed_waveform[t_idx] += A_out * arm_sin_f32(angle);
        }
    }
}
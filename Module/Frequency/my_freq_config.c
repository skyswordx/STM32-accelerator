#include "my_freq_config.h"

extern uint32_t g_ADC_SAMPLE_RATE_Hz; // 2MHz采样率

float32_t g_fft_input_buffer[FFT_LENGTH * 2]; // FFT输入数组，大小为点数的两倍
float32_t g_fft_output_buffer[FFT_LENGTH];    // FFT输出数组，大小等于点数，调用 arm_cfft_f32 后存储的是模值大小
float32_t g_single_magnitude_spectrum[FFT_LENGTH / 2]; // 单边幅度谱数组，大小为点数的一半

fundamental_result_t g_ch1_fundamental; // 基波结果结构
fundamental_result_t g_ch2_fundamental; // 基波结果结构

arm_cfft_radix4_instance_f32 fft_instance_radix4; // FFT实例
arm_rfft_fast_instance_f32 rfft_instance;

/* 窗函数 + FIR滤波器 */
float32_t g_filtered_adc_data[FFT_LENGTH]; // ADC1滤波后的数据
float32_t g_windowed_adc_data[FFT_LENGTH]; // 窗函数处理后的信号

float32_t g_hanning_window[FFT_LENGTH]; // 汉宁窗系数

arm_fir_instance_f32 fir_instance;

// --- FIR滤波器状态缓冲区 ---
// CMSIS-DSP FIR需要一个状态缓冲区来存储历史数据
// 大小为 (滤波器阶数 + 块大小 - 1)
static float32_t fir_state[NUM_TAPS + FFT_LENGTH - 1];

// --- FIR滤波器系数 (从MATLAB导出，时间反转) ---
// CMSIS-DSP FIR函数要求系数是时间反转的 (用MATLAB的fliplr(b)获得)
const float32_t fir_coeffs_reversed[101] = {
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


float32_t caculate_DCcomponent(float32_t* data, uint32_t length) {
    float32_t mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        mean += data[i];
    }
    mean /= length;
    return mean;
}

/**
 * @brief 应用FFT算法
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param enable_window 是否启用窗函数 (1=启用, 0=禁用)
 * @retval None
 * @note 该函数依赖两个全局缓冲区
 */
void my_armcfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, uint8_t enable_window)
{
    // uint8_t ifftFlag = 0;
    // uint8_t doBitReverse = 1;
    arm_cfft_radix4_init_f32(&fft_instance_radix4, FFT_LENGTH, 0, 1);

    float32_t* processing_data = adc_input; // 默认使用原始数据
    uint32_t idx; // 预声明循环变量
    float32_t window_compensation_factor = 1.0f; // 窗函数补偿系数，初始为1

    // --- 可选：FIR滤波步骤 ---
    if (enable_fir) {
        // 初始化FIR滤波器实例
        arm_fir_init_f32(&fir_instance, NUM_TAPS, (float32_t*)fir_coeffs_reversed, fir_state, FFT_LENGTH);
        
        // 应用FIR滤波器
        arm_fir_f32(&fir_instance, adc_input, g_filtered_adc_data, FFT_LENGTH);
        processing_data = g_filtered_adc_data; // 使用滤波后的数据
    } 

    // --- 滤除直流分量（在加窗前进行） ---
    float32_t mean = caculate_DCcomponent(processing_data, FFT_LENGTH);
    for(idx = 0; idx < FFT_LENGTH; idx++) {
        processing_data[idx] = processing_data[idx] - mean; // 就地去除直流分量
    }

    // --- 可选：窗函数步骤 ---
    if (enable_window) {
        // 生成汉宁窗
        float32_t window_sum = 0.0f; // 用于计算窗函数的和
        for(idx = 0; idx < FFT_LENGTH; idx++) {
            g_hanning_window[idx] = 0.5f - 0.5f * arm_cos_f32(2.0f * PI * idx / (FFT_LENGTH - 1));
            window_sum += g_hanning_window[idx]; // 累加窗函数值
        }
        
        // 计算汉宁窗的补偿系数
        // 对于汉宁窗，理论补偿系数约为2.0，但这里用实际计算值更准确
        // window_compensation_factor = (float32_t)((FFT_LENGTH / window_sum)  );
        window_compensation_factor = (float32_t)((FFT_LENGTH / window_sum)  * (HANNING_WINDOW_FACTOR / 2.0f) );
        
        // 应用窗函数
        arm_mult_f32(processing_data, g_hanning_window, g_windowed_adc_data, FFT_LENGTH);
        processing_data = g_windowed_adc_data; // 使用窗函数处理后的数据
        
        // printf("Window applied: compensation factor = %.6f\n", window_compensation_factor);
    } else {
        // printf("No window applied: compensation factor = %.6f\n", window_compensation_factor);
    } 
    
    // 使用处理后的数据进行后续FFT处理
    uint16_t fftLen = fft_instance_radix4.fftLen;
    uint16_t n;
    // 注意：此时已经去除了直流分量，不需要再次计算mean

    for( n = 0; n < fftLen; n++) {
        // 将处理后的输入数据转换为复数格式，实部在偶数索引，虚部在奇数索引
        g_fft_input_buffer[2 * n] = processing_data[n]; // 实部：使用已去直流的数据
        g_fft_input_buffer[2 * n + 1] = 0.0f;     // 虚部
    }

    // 执行FFT
    arm_cfft_radix4_f32(&fft_instance_radix4, g_fft_input_buffer);
    // 计算模值
    arm_cmplx_mag_f32(g_fft_input_buffer, g_fft_output_buffer, fftLen);

    //在模值中寻找基波分量
    float32_t fundamental_magnitude = 0.0f; // 基波幅度
    uint16_t fundamental_index = 0; // 基波索引

    for (uint16_t i = 1; i < fftLen / 2 - 1; i++) {
        if (g_fft_output_buffer[i] > fundamental_magnitude) {
            fundamental_index = i;
            fundamental_magnitude = g_fft_output_buffer[i];
        }
    }

    float32_t fundamental_phase_angle = (atan2f(g_fft_input_buffer[2 * fundamental_index + 1], g_fft_input_buffer[2 * fundamental_index])) * (180.0f / PI) ;

    // 应用窗函数补偿系数来修正幅度
    float32_t corrected_magnitude = fundamental_magnitude * window_compensation_factor;
    
    result->fundamental_vpp = corrected_magnitude * 2.0f / fftLen; // 基波峰峰值（已补偿）
    result->fundamental_vrms = result->fundamental_vpp * sqrtf(2.0f) / 2.0f; // 基波有效值（已补偿）
    result->fundamental_frequency = fundamental_index * (g_ADC_SAMPLE_RATE_Hz / fftLen); // 假设采样率为2MHz
    result->fundamental_phase_angle = fundamental_phase_angle;

    // printf("Raw magnitude: %.6f, Corrected magnitude: %.6f\n", fundamental_magnitude, corrected_magnitude);

    // printf("=== FFT Magnitude Results ===\n");
    // for (uint16_t i = 0; i < fftLen; i++) {     
    //     printf("%.6f,%.6f\n", ((float)(i * g_ADC_SAMPLE_RATE_Hz / fftLen)), g_fft_output_buffer[i]);
    // }

    // 可选：打印滤波后的数据进行调试
    // printf("=== Filtered Data Debug ===\n");
    // for (uint16_t i = 0; i < FFT_LENGTH; i++) {  // 只打印前10个样本
    //     printf("Original/Filtered: %.6f, %.6f\n", adc_input[i], g_filtered_adc_data[i]);
    // }

    // 可选：打印加窗后的数据进行调试
    // printf("=== Windowed Data Debug ===\n");
    // for (uint16_t i = 0; i < FFT_LENGTH; i++) {  // 只打印前10个样本
    //     // printf("Original:%.6f\n", adc_input[i]);
    //     printf("Original/Windowed: %.6f, %.6f\n", adc_input[i], g_windowed_adc_data[i]);
    // }

    // 可选：打印相位谱
    // float32_t phase = 0.0f;
    // printf("=== Phase Spectrum Debug ===\n");
    // for (uint16_t i = 0; i < FFT_LENGTH; i++) {
    //     phase = atan2f(g_fft_input_buffer[2 * i + 1], g_fft_input_buffer[2 * i]) * (180.0f / PI);
    //     printf("%.6f,%.6f\n", (i * g_ADC_SAMPLE_RATE_Hz / FFT_LENGTH), phase);
    // }
}

void my_armrfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, uint8_t enable_window){
    
    float32_t* processing_data = adc_input; // 默认使用原始数据
    uint32_t idx; // 预声明循环变量
    float32_t window_compensation_factor = 1.0f; // 窗函数补偿系数，初始为1
    
    // --- 可选：FIR滤波步骤 ---
    if (enable_fir) {
        // 初始化FIR滤波器实例
        // 参数: 实例指针, 滤波器阶数, 反转的系数指针, 状态缓冲区指针, 块大小
        arm_fir_init_f32(&fir_instance, NUM_TAPS, (float32_t*)fir_coeffs_reversed, fir_state, FFT_LENGTH);//系数在上面有定义，后续可以用MATLAB改一下

        // 应用FIR滤波器
        // 参数: 实例指针, 输入数据指针, 输出数据指针, 块大小
        arm_fir_f32(&fir_instance, adc_input, g_filtered_adc_data, FFT_LENGTH);
        processing_data = g_filtered_adc_data; // 使用滤波后的数据
    } 

    // --- 滤除直流分量（在加窗前进行） ---
    float32_t mean = caculate_DCcomponent(processing_data, FFT_LENGTH);
    for(idx = 0; idx < FFT_LENGTH; idx++) {
        processing_data[idx] = processing_data[idx] - mean; // 就地去除直流分量
    }

    // --- 可选：生成并应用汉宁窗 ---
    if (enable_window) {
        // 生成汉宁窗 (逐点相乘) 
        float32_t window_sum = 0.0f; // 用于计算窗函数的和
        for(idx = 0; idx < FFT_LENGTH; idx++) {
            g_hanning_window[idx] = 0.5f - 0.5f * arm_cos_f32(2.0f * PI * idx / (FFT_LENGTH - 1));
            window_sum += g_hanning_window[idx]; // 累加窗函数值
        }   //DSP库没有窗函数的定义，只能直接算。可能对于固定长度后续可以用SIMD加速
        
        // 计算汉宁窗的补偿系数
        window_compensation_factor = (float32_t)FFT_LENGTH / window_sum;
        
        // 应用汉宁窗
        arm_mult_f32(processing_data, g_hanning_window, g_windowed_adc_data, FFT_LENGTH);
        processing_data = g_windowed_adc_data; // 使用窗函数处理后的数据
        
        printf("Window applied: compensation factor = %.6f\n", window_compensation_factor);
    } else {
        printf("No window applied: compensation factor = %.6f\n", window_compensation_factor);
    } 
    // 

    // --- 执行FFT并计算幅度谱 ---
    // 初始化实数FFT实例
    arm_rfft_4096_fast_init_f32(&rfft_instance);//这里指定了长度，每次计算的时候内部会重新计算旋转因子，库函数中对于特定长度的FFT有特定的优化，可以考虑换成arm_rfft_4096_fast_init_f32

    // 执行FFT。输入是实数，输出是打包的复数格式
    // 参数: 实例指针, 输入数据指针, 输出数据指针, FFT方向标志(0=正向, 1=反向)
    arm_rfft_fast_f32(&rfft_instance, processing_data, g_fft_output_buffer, 0);

    // 计算复数FFT输出的幅度
    // 参数: 输入(打包的复数), 输出(幅度), FFT大小
    arm_cmplx_mag_f32(g_fft_output_buffer, g_single_magnitude_spectrum, FFT_LENGTH / 2);//对于N点实数FFT输出，由于其共轭对称，取numSamples = N/2

    // --- 处理完成 ---
    // 此时, 'magnitude_spectrum' 数组中包含了最终的单边幅度谱。
    // 你可以通过调试器观察这个数组，或者通过UART/SWO将其发送到PC进行绘图验证。
    // 注意：为了得到与MATLAB相同的物理幅度，还需要进行归一化。
    // 例如，除以FFT_SIZE，并对除直流和奈奎斯特频率外的所有分量乘以2。

    //在模值中寻找基波分量
    float32_t fundamental_magnitude = 0.0f; // 基波幅度
    uint16_t fundamental_index = 0; // 基波索引

    for (uint16_t i = 1; i < FFT_LENGTH / 2 - 1; i++) {
        // 注意：这里从1开始，跳过直流分量，避免其影响基波检测
        if (g_single_magnitude_spectrum[i] > fundamental_magnitude) {
            fundamental_index = i;
            fundamental_magnitude = g_single_magnitude_spectrum[i];
        }
    }

    float32_t fundamental_phase_angle = (atan2f(g_single_magnitude_spectrum[2 * fundamental_index + 1], g_single_magnitude_spectrum[2 * fundamental_index])) * (180.0f / PI) ;

    // 应用窗函数补偿系数来修正幅度
    float32_t corrected_magnitude = fundamental_magnitude * window_compensation_factor;
    
    result->fundamental_vpp = corrected_magnitude * 2.0f / FFT_LENGTH; // 基波峰峰值（已补偿）
    result->fundamental_vrms = result->fundamental_vpp * sqrtf(2.0f) / 2.0f; // 基波有效值（已补偿）
    result->fundamental_frequency = fundamental_index * (g_ADC_SAMPLE_RATE_Hz / FFT_LENGTH); // 假设采样率为100kHz
    result->fundamental_phase_angle = fundamental_phase_angle;

    printf("Raw magnitude: %.6f, Corrected magnitude: %.6f\n", fundamental_magnitude, corrected_magnitude);

    for (uint16_t i = 0; i < FFT_LENGTH / 2 - 1; i++) {
        printf("Single-Sided Magnitude: %.6f\n", g_single_magnitude_spectrum[i]);
    }

    // 可选：打印滤波后的数据进行调试
    // printf("=== Filtered Data Debug ===\n");
    // for (uint16_t i = 0; i < FFT_LENGTH; i++) { 
    //     printf("Original/Filtered: %.6f, %.6f\n", adc_input[i], g_filtered_adc_data[i]);
    // }

    // 可选：打印加窗后的数据进行调试
    // printf("=== Windowed Data Debug ===\n");
    // for (uint16_t i = 0; i < FFT_LENGTH; i++) {  // 只打印前10个样本
    //     printf("Original/Windowed: %.6f, %.6f\n", adc_input[i], g_windowed_adc_data[i]);
    // }
}

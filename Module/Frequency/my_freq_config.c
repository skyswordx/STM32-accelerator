#include "my_freq_config.h"
#include <stdint.h>

// Quinn频率估计算法支持
#ifdef ENABLE_QUINN_FREQUENCY_ESTIMATION
#include "my_quinn_config.h"
#endif

extern uint32_t g_ADC_SAMPLE_RATE_Hz; // 2MHz采样率

// 前向声明辅助函数
static arm_status perform_spectral_interpolation(
    float32_t* magnitude_spectrum,
    uint16_t peak_index,
    spectral_interpolation_mode_t mode,
    interpolated_peak_t* result_out
);

/**
 * @brief 执行频谱插值以查找更精确的峰值
 * @param magnitude_spectrum 幅度谱数组 (线性幅度)
 * @param peak_index 检测到的峰值点的索引
 * @param mode 要使用的插值算法模式
 * @param result_out 指向插值结果的结构体指针
 * @retval arm_status ARM_MATH_SUCCESS 如果成功, ARM_MATH_ARGUMENT_ERROR 如果无法插值
 */
static arm_status perform_spectral_interpolation(
    float32_t* magnitude_spectrum,
    uint16_t peak_index,
    spectral_interpolation_mode_t mode,
    interpolated_peak_t* result_out
) {
    // 1. 实现边界检查：如果峰值在频谱的边缘，则无法插值，应返回错误。
    if (peak_index == 0 || peak_index >= (FFT_LENGTH / 2 - 1)) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    
    // 获取相邻点的幅度值
    float32_t y1 = magnitude_spectrum[peak_index - 1];
    float32_t y2 = magnitude_spectrum[peak_index];
    float32_t y3 = magnitude_spectrum[peak_index + 1];
    
    // 2. 使用 switch 或 if-else 结构，根据传入的 mode 参数选择相应的算法。
    switch (mode) {
        // 3. 在 case INTERPOLATION_PARABOLIC 中:
        case INTERPOLATION_PARABOLIC:
            //    - 实现二次抛物线插值算法。
            //    - 提示：此方法通常在对数幅度谱（dB谱）上执行，以获得更好的效果。
            //    - 计算并填充 result_out->corrected_frequency 和 result_out->corrected_magnitude。
            {
                // 转换为对数幅度（dB）
                float32_t d1 = 20.0f * log10f(y1);
                float32_t d2 = 20.0f * log10f(y2);
                float32_t d3 = 20.0f * log10f(y3);
                
                // 二次抛物线插值公式
                float32_t delta = (d1 - d3) / (2.0f * (d1 - 2.0f * d2 + d3));
                
                // 计算校正后的频率和幅度
                result_out->corrected_frequency = (peak_index + delta) * g_ADC_SAMPLE_RATE_Hz / FFT_LENGTH;
                result_out->corrected_magnitude = y2 * powf(10.0f, (d2 + 0.25f * (d1 - d3) * delta) / 20.0f);
            }
            break;
            
        // 4. 在 case INTERPOLATION_HANNING_SPECIAL 中:
        case INTERPOLATION_HANNING_SPECIAL:
            //    - 实现基于汉宁窗特性的插值公式。
            //    - 提示：此方法直接在线性幅度谱上操作，并能同时校正频率和幅度。
            //    - 计算并填充 result_out 的两个字段。
            {
                // 汉宁窗专用插值公式
                float32_t ratio = (y3 - y1) / (2.0f * y2 - y1 - y3);
                
                // 计算校正后的频率和幅度
                result_out->corrected_frequency = (peak_index + ratio) * g_ADC_SAMPLE_RATE_Hz / FFT_LENGTH;
                
                // 汉宁窗幅度校正公式
                float32_t alpha = 0.5f * (2.0f - ratio * ratio);
                result_out->corrected_magnitude = y2 / alpha;
            }
            break;
            
        // 5. 如果 mode 为 INTERPOLATION_DISABLED 或其他无效值，直接返回。
        case INTERPOLATION_DISABLED:
        default:
            return ARM_MATH_ARGUMENT_ERROR;
    }
    
    return ARM_MATH_SUCCESS; // 成功时返回
}

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
float32_t g_flat_top_window[FFT_LENGTH]; // 平顶窗系数

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

// --- 平顶窗系数定义 ---
#define FLAT_TOP_A0 1.000000000000000f
#define FLAT_TOP_A1 1.930000000000000f
#define FLAT_TOP_A2 1.290000000000000f
#define FLAT_TOP_A3 0.388000000000000f
#define FLAT_TOP_A4 0.032000000000000f


float32_t caculate_DCcomponent(float32_t* data, uint32_t length) {
    float32_t mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        mean += data[i];
    }
    mean /= length;
    return mean;
}

/**
 * @brief 生成指定类型的窗函数
 * @param window_type 窗函数类型
 * @param window_buffer 窗函数系数存储缓冲区
 * @param length 窗函数长度
 * @return 窗函数的补偿系数
 */
float32_t generate_window(window_type_t window_type, float32_t* window_buffer, uint32_t length) {
    float32_t window_compensation_factor = 1.0f; // 默认补偿系数
    float32_t window_sum = 0.0f; // 窗函数系数和
    
    switch (window_type) {
        case WINDOW_NONE:
            // 无窗函数，所有系数为1
            for (uint32_t i = 0; i < length; i++) {
                window_buffer[i] = 1.0f;
            }
            window_compensation_factor = 1.0f;
            break;
            
        case WINDOW_HANNING:
            // 生成汉宁窗
            for (uint32_t i = 0; i < length; i++) {
                window_buffer[i] = 0.5f - 0.5f * arm_cos_f32(2.0f * PI * i / (length - 1));
                window_sum += window_buffer[i];
            }
            // 计算汉宁窗补偿系数
            window_compensation_factor = (float32_t)(length / window_sum) * (HANNING_WINDOW_FACTOR / 2.0f);
            break;
            
        case WINDOW_FLAT_TOP:
            // 生成平顶窗 (ISO 18431-2标准)
            for (uint32_t i = 0; i < length; i++) {
                float32_t w = 2.0f * PI * i / (length - 1);
                window_buffer[i] = FLAT_TOP_A0
                                 - FLAT_TOP_A1 * arm_cos_f32(w)
                                 + FLAT_TOP_A2 * arm_cos_f32(2 * w)
                                 - FLAT_TOP_A3 * arm_cos_f32(3 * w)
                                 + FLAT_TOP_A4 * arm_cos_f32(4 * w);
                window_sum += window_buffer[i];
            }
            // 计算平顶窗补偿系数
            window_compensation_factor = (float32_t)(length / window_sum) * FLAT_TOP_WINDOW_FACTOR;
            break;
            
        default:
            // 默认使用无窗函数
            for (uint32_t i = 0; i < length; i++) {
                window_buffer[i] = 1.0f;
            }
            window_compensation_factor = 1.0f;
            break;
    }
    
    return window_compensation_factor;
}

/**
 * @brief 应用复数FFT算法进行频谱分析
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param window_type 窗函数类型:
 *                    - WINDOW_NONE: 不应用窗函数
 *                    - WINDOW_HANNING: 应用汉宁窗
 *                    - WINDOW_FLAT_TOP: 应用平顶窗(适合高精度幅度测量)
 * @param interpolation_mode 频谱插值模式:
 *                          - INTERPOLATION_DISABLED: 不使用插值
 *                          - INTERPOLATION_PARABOLIC: 二次抛物线插值
 *                          - INTERPOLATION_HANNING_SPECIAL: 汉宁窗专用插值
 * @retval None
 * @note 该函数依赖两个全局缓冲区
 */
void my_armcfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, window_type_t window_type, spectral_interpolation_mode_t interpolation_mode)
{
    // uint8_t ifftFlag = 0;
    // uint8_t doBitReverse = 1;
    arm_cfft_radix4_init_f32(&fft_instance_radix4, FFT_LENGTH, 0, 1);

    float32_t* processing_data = adc_input; // 默认使用原始数据
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
    for(uint32_t idx = 0; idx < FFT_LENGTH; idx++) {
        processing_data[idx] = processing_data[idx] - mean; // 就地去除直流分量
    }

    // --- 可选：窗函数步骤 ---
    if (window_type != WINDOW_NONE) {
        // 生成指定类型的窗函数
        window_compensation_factor = generate_window(window_type, g_windowed_adc_data, FFT_LENGTH);
        
        // 应用窗函数
        arm_mult_f32(processing_data, g_windowed_adc_data, g_windowed_adc_data, FFT_LENGTH);
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

    // --- 频谱插值处理 ---
    float32_t final_frequency = fundamental_index * (g_ADC_SAMPLE_RATE_Hz / fftLen); // 默认频率
    float32_t final_magnitude = fundamental_magnitude; // 默认幅度

    // 如果启用了插值且不是禁用模式，则执行插值
    if (interpolation_mode != INTERPOLATION_DISABLED) {
        interpolated_peak_t interpolated_result;
        arm_status status = perform_spectral_interpolation(g_fft_output_buffer, fundamental_index, interpolation_mode, &interpolated_result);
        
        // 如果插值成功，则使用插值结果
        if (status == ARM_MATH_SUCCESS) {
            final_frequency = interpolated_result.corrected_frequency;
            final_magnitude = interpolated_result.corrected_magnitude;
        }
        // 如果插值失败（例如边界条件），则使用原始结果
    }

    // --- Quinn频率估计算法处理 ---
#ifdef ENABLE_QUINN_FREQUENCY_ESTIMATION
    // 只有在启用Quinn算法且模块有效时才使用
    if (g_quinn_module_enabled) {
        quinn_frequency_result_t quinn_result;
        int quinn_status = my_quinn_process(g_fft_input_buffer, fundamental_index, &quinn_result);
        
        // 如果Quinn算法执行成功且结果有效，则使用Quinn算法的频率估计
        if (quinn_status == MY_QUINN_SUCCESS && quinn_result.validity == 1) {
            final_frequency = quinn_result.frequency;
            // printf("Using Quinn frequency estimation: %.2f Hz (confidence: %.2f)\n",
            //        quinn_result.frequency, quinn_result.confidence);
        }
        // 否则使用原有的频率估计（FFT或插值结果）
    }
#endif

    float32_t fundamental_phase_angle = (atan2f(g_fft_input_buffer[2 * fundamental_index + 1], g_fft_input_buffer[2 * fundamental_index])) * (180.0f / PI) ;

    // 应用窗函数补偿系数来修正幅度
    float32_t corrected_magnitude = final_magnitude * window_compensation_factor;
    
    result->fundamental_vpp = corrected_magnitude * 2.0f / fftLen; // 基波峰峰值（已补偿）
    result->fundamental_vrms = result->fundamental_vpp * sqrtf(2.0f) / 2.0f; // 基波有效值（已补偿）
    result->fundamental_frequency = (uint32_t)final_frequency; // 使用插值后的频率
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

/**
 * @brief 应用实数FFT算法进行频谱分析
 * @param adc_input 输入的ADC数据缓冲区(长度为 FFT_LENGTH)
 * @param result 基波分析结果输出结构体指针
 * @param enable_fir 是否启用FIR滤波器 (1=启用, 0=禁用)
 * @param window_type 窗函数类型:
 *                    - WINDOW_NONE: 不应用窗函数
 *                    - WINDOW_HANNING: 应用汉宁窗
 *                    - WINDOW_FLAT_TOP: 应用平顶窗(适合高精度幅度测量)
 * @param interpolation_mode 频谱插值模式:
 *                          - INTERPOLATION_DISABLED: 不使用插值
 *                          - INTERPOLATION_PARABOLIC: 二次抛物线插值
 *                          - INTERPOLATION_HANNING_SPECIAL: 汉宁窗专用插值
 * @retval None
 */
void my_armrfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, window_type_t window_type, spectral_interpolation_mode_t interpolation_mode){
    
    float32_t* processing_data = adc_input; // 默认使用原始数据
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
    for(uint32_t idx = 0; idx < FFT_LENGTH; idx++) {
        processing_data[idx] = processing_data[idx] - mean; // 就地去除直流分量
    }

    // --- 可选：窗函数步骤 ---
    if (window_type != WINDOW_NONE) {
        // 生成指定类型的窗函数
        window_compensation_factor = generate_window(window_type, g_windowed_adc_data, FFT_LENGTH);
        
        // 应用窗函数
        arm_mult_f32(processing_data, g_windowed_adc_data, g_windowed_adc_data, FFT_LENGTH);
        processing_data = g_windowed_adc_data; // 使用窗函数处理后的数据
        
        // printf("Window applied: compensation factor = %.6f\n", window_compensation_factor);
    } else {
        // printf("No window applied: compensation factor = %.6f\n", window_compensation_factor);
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

    // --- 频谱插值处理 ---
    float32_t final_frequency = fundamental_index * (g_ADC_SAMPLE_RATE_Hz / (FFT_LENGTH / 2)); // 默认频率
    float32_t final_magnitude = fundamental_magnitude; // 默认幅度

    // 如果启用了插值且不是禁用模式，则执行插值
    if (interpolation_mode != INTERPOLATION_DISABLED) {
        interpolated_peak_t interpolated_result;
        arm_status status = perform_spectral_interpolation(g_single_magnitude_spectrum, fundamental_index, interpolation_mode, &interpolated_result);
        
        // 如果插值成功，则使用插值结果
        if (status == ARM_MATH_SUCCESS) {
            final_frequency = interpolated_result.corrected_frequency;
            final_magnitude = interpolated_result.corrected_magnitude;
        }
        // 如果插值失败（例如边界条件），则使用原始结果
    }

    // --- Quinn频率估计算法处理 ---
#ifdef ENABLE_QUINN_FREQUENCY_ESTIMATION
    // 只有在启用Quinn算法且模块有效时才使用
    if (g_quinn_module_enabled) {
        quinn_frequency_result_t quinn_result;
        // 注意：对于实数FFT，需要使用不同的FFT数据
        int quinn_status = my_quinn_process(g_fft_output_buffer, fundamental_index, &quinn_result);
        
        // 如果Quinn算法执行成功且结果有效，则使用Quinn算法的频率估计
        if (quinn_status == MY_QUINN_SUCCESS && quinn_result.validity == 1) {
            final_frequency = quinn_result.frequency;
            // printf("Using Quinn frequency estimation: %.2f Hz (confidence: %.2f)\n",
            //        quinn_result.frequency, quinn_result.confidence);
        }
        // 否则使用原有的频率估计（FFT或插值结果）
    }
#endif

    float32_t fundamental_phase_angle = (atan2f(g_fft_output_buffer[2 * fundamental_index + 1], g_fft_output_buffer[2 * fundamental_index])) * (180.0f / PI) ;

    // 应用窗函数补偿系数来修正幅度
    float32_t corrected_magnitude = final_magnitude * window_compensation_factor;
    
    result->fundamental_vpp = corrected_magnitude * 2.0f / FFT_LENGTH; // 基波峰峰值（已补偿）
    result->fundamental_vrms = result->fundamental_vpp * sqrtf(2.0f) / 2.0f; // 基波有效值（已补偿）
    result->fundamental_frequency = (uint16_t)final_frequency; // 使用插值后的频率
    result->fundamental_phase_angle = fundamental_phase_angle;

    // 注释掉调试打印以减少输出
    // for (uint16_t i = 0; i < FFT_LENGTH / 2 - 1; i++) {
    //     printf("Single-Sided Magnitude: %.6f\n", g_single_magnitude_spectrum[i]);
    // }

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
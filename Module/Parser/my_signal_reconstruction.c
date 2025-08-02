#include "my_signal_reconstruction.h"
#include "filter_identification.h" // 为了引用 NUM_FREQ_POINTS 等定义
#include <string.h>
#include <math.h>

// --- 外部依赖 ---
extern uint32_t g_ADC_SAMPLE_RATE_Hz;
extern arm_cfft_radix4_instance_f32 fft_instance_radix4;
extern float32_t g_fft_input_buffer[FFT_LENGTH * 2];
extern float32_t g_fft_output_buffer[FFT_LENGTH];
extern float32_t g_windowed_adc_data[FFT_LENGTH];

extern float32_t generate_window(window_type_t, float32_t*, uint32_t);
extern float32_t caculate_DCcomponent(float32_t*, uint32_t);
extern arm_status perform_spectral_interpolation(float32_t*, uint16_t, spectral_interpolation_mode_t, interpolated_peak_t*);

// --- 私有辅助函数 (Private Helper Functions) ---

/**
 * @brief (私有) 执行通用的FFT分析，找出所有高于阈值的峰值
 */
static int32_t analyze_generic_fft(float32_t* adc_input, harmonic_component_t* components, int32_t max_components) {
    // ... (此函数内部实现与上一版完全相同，此处为节省篇幅省略) ...
    // ... (它执行FFT并返回所有找到的峰值) ...
    arm_cfft_radix4_init_f32(&fft_instance_radix4, FFT_LENGTH, 0, 1);
    float32_t mean = caculate_DCcomponent(adc_input, FFT_LENGTH);
    for(uint32_t i = 0; i < FFT_LENGTH; i++) { g_fft_input_buffer[i] = adc_input[i] - mean; }
    float32_t win_comp = generate_window(WINDOW_HANNING, g_windowed_adc_data, FFT_LENGTH);
    arm_mult_f32(g_fft_input_buffer, g_windowed_adc_data, g_fft_input_buffer, FFT_LENGTH);
    for(int n = FFT_LENGTH - 1; n >= 0; n--) { g_fft_input_buffer[2*n] = g_fft_input_buffer[n]; g_fft_input_buffer[2*n+1] = 0.0f; }
    arm_cfft_radix4_f32(&fft_instance_radix4, g_fft_input_buffer);
    arm_cmplx_mag_f32(g_fft_input_buffer, g_fft_output_buffer, FFT_LENGTH);

    int32_t components_found = 0;
    float32_t fund_mag = 0.0f;
    uint32_t fund_idx = 0;
    arm_max_f32(&g_fft_output_buffer[1], FFT_LENGTH/2 - 1, &fund_mag, &fund_idx);
    fund_idx++;
    const float32_t peak_threshold = fund_mag * 0.01f;

    for (uint16_t i = 1; i < FFT_LENGTH/2 - 1 && components_found < max_components; i++) {
        if (g_fft_output_buffer[i] > g_fft_output_buffer[i-1] && g_fft_output_buffer[i] > g_fft_output_buffer[i+1] && g_fft_output_buffer[i] > peak_threshold) {
            interpolated_peak_t interp_res;
            if (perform_spectral_interpolation(g_fft_output_buffer, i, INTERPOLATION_HANNING_SPECIAL, &interp_res) != ARM_MATH_SUCCESS) {
                interp_res.corrected_frequency = (float32_t)i * g_ADC_SAMPLE_RATE_Hz / FFT_LENGTH;
                interp_res.corrected_magnitude = g_fft_output_buffer[i];
            }
            components[components_found].frequency = interp_res.corrected_frequency;
            float32_t vpp = interp_res.corrected_magnitude * win_comp * 2.0f / FFT_LENGTH;
            components[components_found].amplitude = vpp / 2.0f;
            components[components_found].phase = atan2f(g_fft_input_buffer[2*i+1], g_fft_input_buffer[2*i]);
            components_found++;
        }
    }
    return components_found;
}

/**
 * @brief (私有) 根据数学公式生成理想谐波
 */
static int32_t generate_ideal_harmonics(harmonic_component_t* components, int32_t max_components, wave_type_t type, float32_t freq, float32_t amp) {
    // ... (此函数内部实现与上一版完全相同，此处为节省篇幅省略) ...
    int32_t count = 0;
    memset(components, 0, max_components * sizeof(harmonic_component_t));
    switch (type) {
        case WAVE_SINE:
            components[0].frequency = freq; components[0].amplitude = amp; components[0].phase = 0.0f; count = 1;
            break;
        case WAVE_SQUARE:
            for (int k = 1; count < max_components; k += 2) {
                float32_t current_freq = freq * k;
                if (current_freq > (g_ADC_SAMPLE_RATE_Hz / 2.0f)) break;
                components[count].frequency = current_freq;
                components[count].amplitude = amp * 4.0f / (PI * k);
                components[count].phase = 0.0f;
                count++;
            }
            break;
        case WAVE_TRIANGLE:
            for (int k = 1; count < max_components; k += 2) {
                float32_t current_freq = freq * k;
                if (current_freq > (g_ADC_SAMPLE_RATE_Hz / 2.0f)) break;
                components[count].frequency = current_freq;
                components[count].amplitude = amp * 8.0f / (PI * PI * k * k);
                components[count].phase = (((k - 1) / 2) % 2 != 0) ? PI : 0.0f;
                count++;
            }
            break;
        default: break;
    }
    return count;
}


/**
 * @brief (私有) 根据测量结果自动猜测波形类型
 */
static wave_type_t guess_wave_type(const harmonic_component_t* measured, int32_t num_measured) {
    if (num_measured < 2) return WAVE_SINE; // 只有一个峰值，认为是正弦波

    float32_t even_harmonic_energy = 0.0f;
    float32_t fundamental_freq = measured[0].frequency;

    for (int i = 1; i < num_measured; i++) {
        float32_t ratio = measured[i].frequency / fundamental_freq;
        // 检查是否存在偶次谐波
        if (fabsf(roundf(ratio) - ratio) < 0.1 && (int)roundf(ratio) % 2 == 0) {
            even_harmonic_energy += measured[i].amplitude;
        }
    }

    // 如果偶次谐波的总能量很小（例如小于基波的5%），则认为是方波或三角波
    if (even_harmonic_energy / measured[0].amplitude < 0.05f) {
        // 在此可以进一步通过A3/A1的比例区分方波和三角波，此处简化为方波
        return WAVE_SQUARE;
    }

    return WAVE_UNKNOWN;
}


// --- 公共函数 (Public Functions) ---

analysis_method_t analyze_and_select_best_method(
    harmonic_component_t* final_components,
    int32_t* num_final_components,
    float32_t* adc_input,
    uint8_t enable_math_synthesis
) {
    // 步骤1: 始终执行一次通用FFT，获取“地面实况”
    harmonic_component_t measured_components[MAX_HARMONICS];
    int32_t num_measured = analyze_generic_fft(adc_input, measured_components, MAX_HARMONICS);

    if (num_measured == 0) {
        *num_final_components = 0;
        return METHOD_FAILED;
    }

    // 步骤2: (如果使能) 尝试进行数学合成
    if (enable_math_synthesis) {
        // 2a. 自动猜测波形类型
        wave_type_t guessed_type = guess_wave_type(measured_components, num_measured);

        if (guessed_type == WAVE_SQUARE || guessed_type == WAVE_TRIANGLE) {
            printf("Decision: Signal appears to be a %s wave. Using ideal harmonics.\r\n", guessed_type == WAVE_SQUARE ? "Square" : "Triangle");
            // 2b. 使用测量的基波参数和猜测的类型来生成理想谐波
            *num_final_components = generate_ideal_harmonics(final_components, MAX_HARMONICS, guessed_type, measured_components[0].frequency, measured_components[0].amplitude);
            return METHOD_MATH_SYNTHESIS;
        }
    }

    // 步骤3: (如果数学模型不适用或被禁用) 回退到最安全的“通用FFT”方法
    if (!enable_math_synthesis) {
        printf("Info: Mathematical Synthesis method is disabled by user. ");
    } else {
        printf("Decision: Signal is not a standard wave. ");
    }
    printf("Falling back to Generic FFT analysis.\r\n");
    memcpy(final_components, measured_components, num_measured * sizeof(harmonic_component_t));
    *num_final_components = num_measured;
    return METHOD_GENERIC_FFT;
}


void reconstruct_output_waveform(
    float32_t* reconstructed_waveform,
    uint32_t waveform_points,
    const harmonic_component_t* input_components,
    int32_t num_components,
    const float32_t* w_rad_sweep,
    const float32_t* H_measured_cmplx,
    uint32_t num_sweep_points
) {
    // ... (此函数内部实现与上一版完全相同，此处为节省篇幅省略) ...
    memset(reconstructed_waveform, 0, waveform_points * sizeof(float32_t));
    if (num_components == 0) return;

    float32_t fundamental_freq = input_components[0].frequency;
    float32_t period = 1.0f / fundamental_freq;

    for (int i = 0; i < num_components; i++) {
        float32_t A_in = input_components[i].amplitude;
        float32_t P_in = input_components[i].phase;
        float32_t freq_i = input_components[i].frequency;

        float32_t H_cmplx[2];
        uint32_t index = (uint32_t)roundf((freq_i - SWEEP_F_START_HZ) / SWEEP_F_STEP_HZ);
        
        if (freq_i >= SWEEP_F_START_HZ && freq_i <= SWEEP_F_STOP_HZ && index < num_sweep_points) {
             H_cmplx[0] = H_measured_cmplx[index * 2];
             H_cmplx[1] = H_measured_cmplx[index * 2 + 1];
        } else {
            continue;
        }

        float32_t gain, P_filter;
        arm_cmplx_mag_f32(H_cmplx, &gain, 1);
        P_filter = atan2f(H_cmplx[1], H_cmplx[0]);
        
        float32_t A_out = A_in * gain;
        float32_t P_out = P_in + P_filter;

        for (uint32_t t_idx = 0; t_idx < waveform_points; t_idx++) {
            float32_t t = (float32_t)t_idx * period / waveform_points;
            reconstructed_waveform[t_idx] += A_out * arm_sin_f32(2.0f * PI * freq_i * t + P_out);
        }
    }
}

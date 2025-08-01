#include "my_signal_suite.h"
#include "arm_math.h"
#include <string.h> // For memset

/**
 * @brief (內部函數) 在FFT模值譜中尋找最接近目標頻率的峰值索引
 */
static uint16_t find_peak_index_near_freq(const float32_t* magnitude_spectrum, uint32_t fft_len, uint32_t sample_rate, float32_t target_freq_hz) {
    float32_t freq_resolution = (float32_t)sample_rate / fft_len;
    uint16_t target_bin = (uint16_t)roundf(target_freq_hz / freq_resolution);

    // 為了穩健性，在目標bin附近的一個小窗口內搜索最大值
    // 這可以防止因頻率洩漏導致的微小偏差
    uint16_t search_radius = 2; 
    uint16_t start_bin = (target_bin > search_radius) ? (target_bin - search_radius) : 1; // 從bin 1開始，避開直流
    uint16_t end_bin = (target_bin + search_radius) < (fft_len / 2) ? (target_bin + search_radius) : (fft_len / 2 - 1);
    
    float32_t max_mag = 0.0f;
    uint16_t peak_index = 0;

    for (uint16_t i = start_bin; i <= end_bin; i++) {
        if (magnitude_spectrum[i] > max_mag) {
            max_mag = magnitude_spectrum[i];
            peak_index = i;
        }
    }
    return peak_index;
}


void analyze_signal_with_harmonics(
    uint32_t fft_len,
    uint32_t sample_rate,
    float32_t window_compensation_factor,
    const float32_t* complex_fft_output,
    const float32_t* magnitude_spectrum,
    SignalAnalysisResult* result_out)
{
    // 清空結果結構體
    memset(result_out, 0, sizeof(SignalAnalysisResult));

    float32_t freq_resolution = (float32_t)sample_rate / fft_len;

    // 1. 尋找基波 (整個頻譜中的最大峰值)
    uint16_t fundamental_index = 0;
    float32_t fundamental_magnitude = 0.0f;
    // 從索引1開始搜索，忽略直流分量
    for (uint16_t i = 1; i < fft_len / 2; i++) {
        if (magnitude_spectrum[i] > fundamental_magnitude) {
            fundamental_magnitude = magnitude_spectrum[i];
            fundamental_index = i;
        }
    }
    
    if (fundamental_index == 0) {
        // 沒有找到有效的基波信號
        return;
    }

    // 2. 提取基波資訊
    // 注意：這裡不使用插值，因為您現有的高精度方案已在 my_armcfft32_apply 中完成。
    // 這個新函數的目標是基於已有的FFT結果進行諧波提取。
    float32_t fundamental_freq_hz = (float32_t)fundamental_index * freq_resolution;
    float32_t fundamental_phase_rad = atan2f(complex_fft_output[2 * fundamental_index + 1], complex_fft_output[2 * fundamental_index]);
    float32_t fundamental_amp_vpp = fundamental_magnitude * window_compensation_factor * 2.0f / fft_len;

    result_out->fundamental.fundamental_frequency = (uint32_t)fundamental_freq_hz;
    result_out->fundamental.fundamental_phase = fundamental_phase_rad;
    result_out->fundamental.fundamental_vpp = fundamental_amp_vpp;
    result_out->fundamental.fundamental_vrms = fundamental_amp_vpp / (2.0f * arm_sqrt_f32(2.0f));


    // 3. 尋找並提取諧波資訊
    result_out->num_harmonics_found = 0;
    for (uint8_t n = 2; n <= (MAX_HARMONICS_TO_ANALYZE + 1); n++) {
        float32_t target_harmonic_freq = fundamental_freq_hz * n;
        
        // 檢查目標諧波是否超過奈奎斯特頻率
        if (target_harmonic_freq > (sample_rate / 2.0f)) {
            break;
        }

        uint16_t harmonic_index = find_peak_index_near_freq(magnitude_spectrum, fft_len, sample_rate, target_harmonic_freq);

        if (harmonic_index > 0) {
             // 為了避免將噪聲誤判為諧波，可以設定一個閾值
             // 例如，如果諧波幅度小於基波的0.1%，則忽略
            float32_t harmonic_magnitude = magnitude_spectrum[harmonic_index];
            if (harmonic_magnitude < fundamental_magnitude * 0.001f) {
                continue;
            }

            uint8_t current_harmonic_idx = result_out->num_harmonics_found;

            float32_t harmonic_phase_rad = atan2f(complex_fft_output[2 * harmonic_index + 1], complex_fft_output[2 * harmonic_index]);
            float32_t harmonic_amp_vpp = harmonic_magnitude * window_compensation_factor * 2.0f / fft_len;

            result_out->harmonics[current_harmonic_idx].harmonic_order = n;
            result_out->harmonics[current_harmonic_idx].frequency_hz = (float32_t)harmonic_index * freq_resolution;
            result_out->harmonics[current_harmonic_idx].amplitude_vpp = harmonic_amp_vpp;
            result_out->harmonics[current_harmonic_idx].phase_rad = harmonic_phase_rad;
            
            result_out->num_harmonics_found++;
        }
    }
}


void calculate_system_response_at_freq(
    const ContinuousTransferFunction* tf,
    float32_t freq_hz,
    float32_t* gain,
    float32_t* phase_shift_rad)
{
    // H(s) = (b2*s^2 + b1*s + b0) / (s^2 + a1*s + a0)
    //  sustitude s = jω, where ω = 2*pi*f
    // H(jω) = (b0 - b2*ω^2 + j*b1*ω) / (a0 - ω^2 + j*a1*ω)

    float32_t w = 2.0f * PI * freq_hz;
    float32_t w_sq = w * w;

    // Numerator (分子) N = num_real + j * num_imag
    float32_t num_real = tf->b0 - tf->b2 * w_sq;
    float32_t num_imag = tf->b1 * w;

    // Denominator (分母) D = den_real + j * den_imag
    float32_t den_real = tf->a0 - w_sq;
    float32_t den_imag = tf->a1 * w;

    // Calculate Magnitudes |N| and |D|
    float32_t mag_num, mag_den;
    arm_sqrt_f32(num_real * num_real + num_imag * num_imag, &mag_num);
    arm_sqrt_f32(den_real * den_real + den_imag * den_imag, &mag_den);

    // Calculate Gain = |N| / |D|
    if (mag_den < 1e-9f) { // 防止除以零
        *gain = 0.0f;
    } else {
        *gain = mag_num / mag_den;
    }

    // Calculate Phases arg(N) and arg(D)
    float32_t phase_num = atan2f(num_imag, num_real);
    float32_t phase_den = atan2f(den_imag, den_real);

    // Calculate Phase Shift = arg(N) - arg(D)
    *phase_shift_rad = phase_num - phase_den;
}


void reconstruct_output_waveform(
    const SignalAnalysisResult* analysis_result,
    const ContinuousTransferFunction* system_tf,
    float32_t* output_waveform)
{
    // 1. 清空輸出緩衝區
    arm_fill_f32(0.0f, output_waveform, RECONSTRUCTED_WAVEFORM_SIZE);

    float32_t gain, phase_shift;
    
    // 2. 處理基波分量
    // 獲取基波在被測系統上的增益和相移
    calculate_system_response_at_freq(system_tf, (float32_t)analysis_result->fundamental.fundamental_frequency, &gain, &phase_shift);
    
    float32_t new_amp_peak = (analysis_result->fundamental.fundamental_vpp / 2.0f) * gain;
    float32_t new_phase = analysis_result->fundamental.fundamental_phase + phase_shift;

    // 在時域上合成基波
    for (uint16_t i = 0; i < RECONSTRUCTED_WAVEFORM_SIZE; i++) {
        // n=1 for fundamental
        float32_t angle = 2.0f * PI * 1.0f * i / RECONSTRUCTED_WAVEFORM_SIZE + new_phase;
        output_waveform[i] += new_amp_peak * arm_cos_f32(angle);
    }

    // 3. 處理所有諧波分量
    for (uint8_t k = 0; k < analysis_result->num_harmonics_found; k++) {
        const HarmonicComponent* harmonic = &analysis_result->harmonics[k];

        // 獲取此諧波在被測系統上的增益和相移
        calculate_system_response_at_freq(system_tf, harmonic->frequency_hz, &gain, &phase_shift);

        new_amp_peak = (harmonic->amplitude_vpp / 2.0f) * gain;
        new_phase = harmonic->phase_rad + phase_shift;
        
        // 在時域上累加合成諧波
        for (uint16_t i = 0; i < RECONSTRUCTED_WAVEFORM_SIZE; i++) {
            float32_t angle = 2.0f * PI * harmonic->harmonic_order * i / RECONSTRUCTED_WAVEFORM_SIZE + new_phase;
            output_waveform[i] += new_amp_peak * arm_cos_f32(angle);
        }
    }
}
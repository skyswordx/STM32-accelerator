#ifndef SIGNAL_PROCESSING_SUITE_H
#define SIGNAL_PROCESSING_SUITE_H

#include "filter_identification.h"
#include "my_freq_config.h" // 為了使用 fundamental_result_t
#include "arm_math.h"
#include <stdint.h>

// --- 可配置宏定義 ---

// 1. 您希望分析到第幾次諧波？ (例如，9次諧波，即分析到基波的10倍頻)
#define MAX_HARMONICS_TO_ANALYZE 9

// 2. 您希望重建的波形一個週期包含多少個取樣點？
//    這個值可以根據您的 DAC 輸出需求來設定，例如 1024 或 2048
#define RECONSTRUCTED_WAVEFORM_SIZE 4096


// --- 新的資料結構 ---

/**
 * @brief 存儲單個諧波分量的資訊
 */
typedef struct {
    uint8_t   harmonic_order;    // 諧波次數 (例如 2, 3, 4...)
    float32_t frequency_hz;      // 諧波頻率 (Hz)
    float32_t amplitude_vpp;     // 諧波幅度 (Vpp)
    float32_t phase_rad;         // 諧波相位 (弧度)
} HarmonicComponent;

/**
 * @brief 存儲完整信號分析的結果，包括基波和所有諧波
 */
typedef struct {
    fundamental_result_t fundamental; // 基波資訊
    HarmonicComponent harmonics[MAX_HARMONICS_TO_ANALYZE]; // 諧波資訊陣列
    uint8_t num_harmonics_found; // 實際找到的諧波數量
} SignalAnalysisResult;


// --- 公共函數原型 ---

/**
 * @brief 要求2: 分析信號，提取基波和多次諧波的幅度和相位
 * @param adc_input 指向輸入ADC數據的指標
 * @param fft_len FFT的長度 (例如: 4096)
 * @param sample_rate ADC的採樣率 (Hz)
 * @param window_compensation_factor 使用的窗函數補償係數
 * @param complex_fft_output 執行完 arm_cfft_radix4_f32 後的複數FFT結果緩衝區
 * @param magnitude_spectrum 複數FFT結果對應的模值譜
 * @param result_out 指向存儲分析結果的結構體
 * @note 該函數假設FFT和模值計算已經在外部完成
 */
void analyze_signal_with_harmonics(
    uint32_t fft_len,
    uint32_t sample_rate,
    float32_t window_compensation_factor,
    const float32_t* complex_fft_output,
    const float32_t* magnitude_spectrum,
    SignalAnalysisResult* result_out
);

/**
 * @brief (輔助功能) 根據已辨識的傳遞函數，計算在特定頻率下的增益和相移
 * @param tf 指向已辨識的傳遞函數係數的指標
 * @param freq_hz 需要計算響應的頻率 (Hz)
 * @param gain 指向用於存儲計算出的增益(線性值)的變數指標
 * @param phase_shift_rad 指向用於存儲計算出的相移(弧度)的變數指標
 */
void calculate_system_response_at_freq(
    const ContinuousTransferFunction* tf,
    float32_t freq_hz,
    float32_t* gain,
    float32_t* phase_shift_rad
);

/**
 * @brief 要求3: 重建經過被測系統影響後的輸出波形
 * @param analysis_result 包含基波和諧波資訊的信號分析結果
 * @param system_tf 描述被測系統的傳遞函數
 * @param output_waveform 指向用於存儲重建波形的陣列
 * @param waveform_size output_waveform 陣列的大小
 * @param sample_rate 用於重建波形的採樣率 (應與分析時的採樣率一致)
 */
void reconstruct_output_waveform(
    const SignalAnalysisResult* analysis_result,
    const ContinuousTransferFunction* system_tf,
    float32_t* output_waveform,
    uint32_t waveform_size,
    uint32_t sample_rate
);

#endif // SIGNAL_PROCESSING_SUITE_H
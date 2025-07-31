/* signal_analyzer.h */
#ifndef SIGNAL_ANALYZER_H
#define SIGNAL_ANALYZER_H

#include "arm_math.h" // 引入 CMSIS-DSP 函式庫

// --- Type Definitions ---
// SOGI-FLL 狀態結構
typedef struct {
    float32_t v_alpha, v_beta;
    float32_t v_alpha_prev, v_beta_prev;
    float32_t integrator;
    float32_t freq_err;
    float32_t k_p, k_i; // PI 控制器增益
    float32_t gamma;    // SOGI 阻尼係數
} SOGI_FLL_t;

// 同步檢波器狀態結構
typedef struct {
    float32_t i_component; // 同相分量
    float32_t q_component; // 正交分量
    float32_t lpf_alpha;   // 低通濾波器係數
    uint64_t phase_accumulator; // 用於產生本地參考信號
} SyncDetector_t;

// 主分析儀控制代碼
typedef struct {
    // --- 系統參數 ---
    float32_t sample_rate;
    float32_t adc_ref_voltage;
    float32_t adc_resolution_factor;

    // --- 狀態變數 ---
    float32_t dc_offset;
    SOGI_FLL_t fll;
    SyncDetector_t detector;

    // --- 結果 ---
    float32_t estimated_frequency;
    float32_t estimated_vpp;
    float32_t estimated_phase_rad;

} SignalAnalyzer_t;


// --- Public API Functions ---

/**
 * @brief 初始化信號分析儀。
 * @param analyzer 指向 SignalAnalyzer_t 控制代碼的指標。
 * @param sample_rate ADC的採樣率 (Hz)。
 * @param initial_freq FLL的初始鎖定頻率 (Hz)。
 * @param adc_ref_v ADC的參考電壓。
 * @param adc_res_factor ADC的解析度因子 (例如 12位元為 4096.0f)。
 */
void SignalAnalyzer_Init(SignalAnalyzer_t* analyzer, float32_t sample_rate, float32_t initial_freq, float32_t adc_ref_v, float32_t adc_res_factor);

/**
 * @brief **NEW**: 重置分析儀的內部狀態以進行新的獨立測量。
 * @param analyzer 指向 SignalAnalyzer_t 控制代碼的指標。
 * @param initial_freq 新測量開始時的初始猜測頻率。
 */
void SignalAnalyzer_Reset(SignalAnalyzer_t* analyzer, float32_t initial_freq);

/**
 * @brief 使用一個數據塊來校準直流偏置。
 * @param analyzer 指向 SignalAnalyzer_t 控制代碼的指標。
 * @param buffer 指向包含ADC原始數據的緩衝區。
 * @param length 緩衝區的長度。
 */
void SignalAnalyzer_CalibrateDCOffset(SignalAnalyzer_t* analyzer, const uint16_t* buffer, uint32_t length);

/**
 * @brief 處理一個新的數據塊。
 * @param analyzer 指向 SignalAnalyzer_t 控制代碼的指標。
 * @param buffer 指向包含ADC原始數據的緩衝區。
 * @param length 緩衝區的長度。
 */
void SignalAnalyzer_ProcessBuffer(SignalAnalyzer_t* analyzer, const uint16_t* buffer, uint32_t length);

// --- Getter Functions ---
float32_t SignalAnalyzer_GetFrequency(const SignalAnalyzer_t* analyzer);
float32_t SignalAnalyzer_GetVpp(const SignalAnalyzer_t* analyzer);
float32_t SignalAnalyzer_GetPhase(const SignalAnalyzer_t* analyzer);


#endif // SIGNAL_ANALYZER_H

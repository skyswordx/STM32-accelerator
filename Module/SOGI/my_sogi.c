
/* signal_analyzer.c */
#include "my_sogi.h"
#include <string.h>

// --- Private Function Prototypes ---
static void SOGI_FLL_Update(SignalAnalyzer_t* analyzer, float32_t sample);
static void SyncDetector_Update(SignalAnalyzer_t* analyzer, float32_t sample);

// --- Public Function Implementations ---

void SignalAnalyzer_Init(SignalAnalyzer_t* analyzer, float32_t sample_rate, float32_t initial_freq, float32_t adc_ref_v, float32_t adc_res_factor) {
    memset(analyzer, 0, sizeof(SignalAnalyzer_t));

    analyzer->sample_rate = sample_rate;
    analyzer->adc_ref_voltage = adc_ref_v;
    analyzer->adc_resolution_factor = adc_res_factor;
    
    // 初始化 SOGI-FLL 參數 (這些參數需要根據你的需求進行調整)
    analyzer->fll.gamma = 1.414f; // 阻尼係數
    analyzer->fll.k_p = 200.0f;   // PI比例增益
    analyzer->fll.k_i = 0.005f;   // PI積分增益

    // 初始化同步檢波器
    analyzer->detector.lpf_alpha = 0.005f; // I/Q 低通濾波器的平滑係數

    // 呼叫Reset函式來設定初始狀態
    SignalAnalyzer_Reset(analyzer, initial_freq);
}

// ** NEW FUNCTION IMPLEMENTATION **
void SignalAnalyzer_Reset(SignalAnalyzer_t* analyzer, float32_t initial_freq) {
    // 重置 FLL 狀態
    analyzer->fll.v_alpha = 0.0f;
    analyzer->fll.v_beta = 0.0f;
    analyzer->fll.v_alpha_prev = 0.0f;
    analyzer->fll.v_beta_prev = 0.0f;
    analyzer->fll.integrator = 0.0f;
    analyzer->fll.freq_err = 0.0f;

    // 重置同步檢波器狀態
    analyzer->detector.i_component = 0.0f;
    analyzer->detector.q_component = 0.0f;
    analyzer->detector.phase_accumulator = 0;

    // 重置結果
    analyzer->estimated_frequency = initial_freq;
    analyzer->estimated_vpp = 0.0f;
    analyzer->estimated_phase_rad = 0.0f;
}

void SignalAnalyzer_CalibrateDCOffset(SignalAnalyzer_t* analyzer, const uint16_t* buffer, uint32_t length) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += buffer[i];
    }
    analyzer->dc_offset = (float32_t)sum / length;
}

void SignalAnalyzer_ProcessBuffer(SignalAnalyzer_t* analyzer, const uint16_t* buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        // 將ADC原始值轉換為浮點數並移除直流偏置
        float32_t sample = (float32_t)buffer[i] - analyzer->dc_offset;

        // 1. 更新SOGI-FLL以獲得精確的頻率
        SOGI_FLL_Update(analyzer, sample);

        // 2. 使用新頻率更新同步檢波器
        SyncDetector_Update(analyzer, sample);
    }

    // 3. 在處理完一個塊後，計算最終結果
    float32_t i_val = analyzer->detector.i_component;
    float32_t q_val = analyzer->detector.q_component;
    float32_t magnitude = 0.0f;
    arm_sqrt_f32(i_val * i_val + q_val * q_val, &magnitude);

    // 將ADC code轉換為電壓
    // 這裡的 'magnitude' 是峰值的一半，因為我們與sin/cos(幅值為1)混頻
    float32_t v_peak = 2.0f * magnitude * (analyzer->adc_ref_voltage / analyzer->adc_resolution_factor);
    analyzer->estimated_vpp = 2.0f * v_peak;
    analyzer->estimated_phase_rad = atan2f(q_val, i_val);
}

// --- Getter Functions ---
float32_t SignalAnalyzer_GetFrequency(const SignalAnalyzer_t* analyzer) {
    return analyzer->estimated_frequency;
}
float32_t SignalAnalyzer_GetVpp(const SignalAnalyzer_t* analyzer) {
    return analyzer->estimated_vpp;
}
float32_t SignalAnalyzer_GetPhase(const SignalAnalyzer_t* analyzer) {
    return analyzer->estimated_phase_rad;
}

// --- Private Function Implementations ---

static void SOGI_FLL_Update(SignalAnalyzer_t* analyzer, float32_t sample) {
    SOGI_FLL_t* fll = &analyzer->fll;
    float32_t w = 2.0f * PI * analyzer->estimated_frequency / analyzer->sample_rate;

    // SOGI 結構
    fll->v_alpha = fll->v_alpha_prev + w * fll->v_beta_prev;
    fll->v_beta = fll->v_beta_prev - w * fll->v_alpha;
    
    float32_t v_err = sample - fll->v_alpha;
    fll->v_alpha = fll->v_alpha + fll->gamma * w * v_err;
    
    fll->v_alpha_prev = fll->v_alpha;
    fll->v_beta_prev = fll->v_beta;

    // FLL 結構
    fll->freq_err = -v_err * fll->v_beta;
    fll->integrator += fll->k_i * fll->freq_err;
    analyzer->estimated_frequency += fll->k_p * fll->freq_err + fll->integrator;

    // 頻率限制
    if (analyzer->estimated_frequency > 55000.0f) analyzer->estimated_frequency = 55000.0f;
    if (analyzer->estimated_frequency < 90.0f) analyzer->estimated_frequency = 90.0f;
}

static void SyncDetector_Update(SignalAnalyzer_t* analyzer, float32_t sample) {
    SyncDetector_t* detector = &analyzer->detector;

    // 1. 產生本地參考信號 (使用DDS原理)
    uint64_t fcw = (uint64_t)((double)analyzer->estimated_frequency * (double)(1ULL << 63) * 2.0 / (double)analyzer->sample_rate);
    detector->phase_accumulator += fcw;
    
    // 這裡我們需要一個快速的sin/cos。為了簡化，我們用一個近似。
    // 在實際應用中，可以使用查表法或CMSIS-DSP的函式。
    // 這裡使用一個簡單的DDS相位累加器，並假設我們有一個快速的sin/cos實現。
    // 為了讓程式碼能跑，我們用arm_sin/cos，但注意其效能。
    float32_t phase_rad = (float32_t)(detector->phase_accumulator >> 32) * (2.0f * PI / (1ULL << 32));
    float32_t ref_sin = arm_sin_f32(phase_rad);
    float32_t ref_cos = arm_cos_f32(phase_rad);

    // 2. 混頻
    float32_t mix_i = sample * ref_sin;
    float32_t mix_q = sample * ref_cos;

    // 3. 低通濾波 (簡單的一階 IIR LPF)
    detector->i_component += detector->lpf_alpha * (mix_i - detector->i_component);
    detector->q_component += detector->lpf_alpha * (mix_q - detector->q_component);
}
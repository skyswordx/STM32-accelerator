#include "my_dila_config.h"
#include "arm_math.h"
#include "math.h"
#include "string.h"

// --- 信号缓冲区 ---
// 输入信号
static float32_t signal_in[DILA_N];

// 理想参考信号
static float32_t ref_I_ideal[DILA_N];
static float32_t ref_Q_ideal[DILA_N];

// 滤波后的信号
static float32_t signal_filtered[DILA_N];
static float32_t ref_I_filtered[DILA_N];
static float32_t ref_Q_filtered[DILA_N];

// 解调后的I/Q信号 (混频后)
static float32_t I_raw[DILA_N];
static float32_t Q_raw[DILA_N];

// 最终滤波后的I/Q信号 (直流分量)
static float32_t I_filtered[DILA_N];
static float32_t Q_filtered[DILA_N];

// --- FIR滤波器实例 ---
// 输入滤波器实例 (信号, I参考, Q参考 共用相同的系数)
static arm_fir_instance_f32 fir_inst_input_signal;
static arm_fir_instance_f32 fir_inst_input_ref_i;
static arm_fir_instance_f32 fir_inst_input_ref_q;

// 输出滤波器实例 (I通道, Q通道 共用相同的系数)
static arm_fir_instance_f32 fir_inst_output_i;
static arm_fir_instance_f32 fir_inst_output_q;

// --- FIR滤波器状态缓冲区 ---
// 状态缓冲区大小为 (抽头数 + 块大小 - 1)
#define INPUT_FIR_STATE_SIZE  (INPUT_FIR_NUM_TAPS + DILA_N - 1)
#define OUTPUT_FIR_STATE_SIZE (OUTPUT_FIR_NUM_TAPS + DILA_N - 1)

static float32_t fir_state_input_signal[INPUT_FIR_STATE_SIZE];
static float32_t fir_state_input_ref_i[INPUT_FIR_STATE_SIZE];
static float32_t fir_state_input_ref_q[INPUT_FIR_STATE_SIZE];
static float32_t fir_state_output_i[OUTPUT_FIR_STATE_SIZE];
static float32_t fir_state_output_q[OUTPUT_FIR_STATE_SIZE];

/**
 * @brief 初始化DILA模块
 * @details 初始化FIR滤波器实例和相关参数
 * @param[in] input_fir_coeffs 输入FIR滤波器系数数组
 * @param[in] output_fir_coeffs 输出FIR滤波器系数数组
 * @return 0表示成功，其他值表示失败
 */
int my_dila_init(const float32_t* input_fir_coeffs, const float32_t* output_fir_coeffs)
{
    // 检查输入参数
    if (input_fir_coeffs == NULL || output_fir_coeffs == NULL) {
        return MY_DILA_ERROR_INVALID_PARAMETER;
    }
    
    // 初始化输入FIR滤波器
    arm_fir_init_f32(&fir_inst_input_signal, INPUT_FIR_NUM_TAPS, (float32_t*)input_fir_coeffs, fir_state_input_signal, DILA_N);
    arm_fir_init_f32(&fir_inst_input_ref_i, INPUT_FIR_NUM_TAPS, (float32_t*)input_fir_coeffs, fir_state_input_ref_i, DILA_N);
    arm_fir_init_f32(&fir_inst_input_ref_q, INPUT_FIR_NUM_TAPS, (float32_t*)input_fir_coeffs, fir_state_input_ref_q, DILA_N);
    
    // 初始化输出FIR滤波器
    arm_fir_init_f32(&fir_inst_output_i, OUTPUT_FIR_NUM_TAPS, (float32_t*)output_fir_coeffs, fir_state_output_i, DILA_N);
    arm_fir_init_f32(&fir_inst_output_q, OUTPUT_FIR_NUM_TAPS, (float32_t*)output_fir_coeffs, fir_state_output_q, DILA_N);
    
    return MY_DILA_SUCCESS;
}

/**
 * @brief 生成理想参考信号
 * @details 根据配置参数生成正交的正弦和余弦参考信号
 */
void my_dila_generate_reference_signals(void)
{
    // 计算时间步长
    float32_t dt = 1.0f / DILA_FS;
    
    // 生成正交的正弦和余弦参考信号
    for(uint16_t i = 0; i < DILA_N; i++)
    {
        float32_t t = i * dt;
        ref_I_ideal[i] = arm_sin_f32(2 * PI * DILA_FIN_ACTUAL * t);
        ref_Q_ideal[i] = arm_cos_f32(2 * PI * DILA_FIN_ACTUAL * t);
    }
}

/**
 * @brief 执行DILA核心处理流程
 * @details 包括输入滤波、IQ解调、输出滤波等步骤
 * @param[in] input_signal 输入信号数组
 */
void my_dila_process_signal(const float32_t* input_signal)
{
    // 1. 将输入信号复制到内部缓冲区
    // 这一步是为了确保输入信号不会在处理过程中被修改
    memcpy(signal_in, input_signal, sizeof(float32_t) * DILA_N);
    
    // 2. 输入滤波: 信号和参考信号通过完全相同的输入滤波器
    // 这一步是为了匹配输入信号和参考信号的相位响应
    arm_fir_f32(&fir_inst_input_signal, signal_in, signal_filtered, DILA_N);
    arm_fir_f32(&fir_inst_input_ref_i, ref_I_ideal, ref_I_filtered, DILA_N);
    arm_fir_f32(&fir_inst_input_ref_q, ref_Q_ideal, ref_Q_filtered, DILA_N);
    
    // 3. I/Q解调 (使用滤波后的参考信号)
    // 将输入信号与正交参考信号相乘，得到I/Q分量
    arm_mult_f32(signal_filtered, ref_I_filtered, I_raw, DILA_N);
    arm_mult_f32(signal_filtered, ref_Q_filtered, Q_raw, DILA_N);
    
    // 4. 输出滤波 (滤除2*Fin分量)
    // 使用窄带滤波器滤除高频分量，保留直流分量
    arm_fir_f32(&fir_inst_output_i, I_raw, I_filtered, DILA_N);
    arm_fir_f32(&fir_inst_output_q, Q_raw, Q_filtered, DILA_N);
}

/**
 * @brief 计算幅度和相位结果
 * @details 从滤波后的I/Q信号中计算幅度和相位
 * @param[out] magnitude 输出幅度值
 * @param[out] phase 输出相位值(弧度)
 * @param[out] phase_deg 输出相位值(度)
 */
void my_dila_calculate_results(float32_t* magnitude, float32_t* phase, float32_t* phase_deg)
{
    float32_t I_dc, Q_dc;
    
    // 1. 计算均值
    // 丢弃瞬态部分，仅对稳态部分求均值
    uint16_t start_idx = DILA_TOTAL_DELAY;
    uint32_t steady_state_len = DILA_N - start_idx;
    
    // 检查稳态长度是否有效
    if (steady_state_len <= 0)
    {
        // 错误处理
        return;
    }
    
    // 计算I/Q通道的直流分量
    arm_mean_f32(&I_filtered[start_idx], steady_state_len, &I_dc);
    arm_mean_f32(&Q_filtered[start_idx], steady_state_len, &Q_dc);
    
    // 2. 幅度计算
    // R_meas = 2 * sqrt(I_dc^2 + Q_dc^2)
    float32_t temp_sqrt_arg = I_dc * I_dc + Q_dc * Q_dc;
    arm_sqrt_f32(temp_sqrt_arg, magnitude);
    *magnitude *= 2.0f;
    
    // 3. 相位计算
    // 使用atan2f函数计算相位，范围为(-π, π]
    *phase = atan2f(Q_dc, I_dc);
    *phase_deg = *phase * 180.0f / PI;
}

/**
 * @brief 获取I通道滤波后的信号
 * @param[out] buffer 输出缓冲区
 * @param[in] length 缓冲区长度
 */
void my_dila_get_I_filtered(float32_t* buffer, size_t length)
{
    // 检查输入参数
    if (buffer == NULL || length > DILA_N) {
        return;
    }
    // 复制数据到输出缓冲区
    memcpy(buffer, I_filtered, sizeof(float32_t) * length);
}

/**
 * @brief 获取Q通道滤波后的信号
 * @param[out] buffer 输出缓冲区
 * @param[in] length 缓冲区长度
 */
void my_dila_get_Q_filtered(float32_t* buffer, size_t length)
{
    // 检查输入参数
    if (buffer == NULL || length > DILA_N) {
        return;
    }
    // 复制数据到输出缓冲区
    memcpy(buffer, Q_filtered, sizeof(float32_t) * length);
}
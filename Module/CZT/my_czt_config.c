#include "my_czt_config.h"
#include <string.h>
#include <math.h>

// --- 信号缓冲区 ---
// 输入信号 (实数)
static float32_t signal_in[CZT_N];

// 输入信号 (复数)
static float32_t signal_in_real[CZT_N];
static float32_t signal_in_imag[CZT_N];

// 输出信号 (复数)
static float32_t signal_out_real[CZT_M];
static float32_t signal_out_imag[CZT_M];

// Chirp序列
static float32_t chirp_sequence_real[CZT_N + CZT_M - 1];
static float32_t chirp_sequence_imag[CZT_N + CZT_M - 1];

// Chirp序列的逆
static float32_t chirp_sequence_inv_real[CZT_M];
static float32_t chirp_sequence_inv_imag[CZT_M];

// 中间结果
static float32_t intermediate_real[CZT_N + CZT_M - 1];
static float32_t intermediate_imag[CZT_N + CZT_M - 1];

// FFT输入缓冲区 (复数格式: 实部和虚部交替存储)
static float32_t fft_input_buffer[(CZT_N + CZT_M - 1) * 2];

// --- FFT实例 ---
static arm_cfft_radix4_instance_f32 cfft_inst;

/**
 * @brief 初始化CZT模块
 * @details 初始化FFT实例和相关参数
 * @return 0表示成功，其他值表示失败
 */
int my_czt_init(void)
{
    // 初始化FFT实例
    arm_status status = arm_cfft_radix4_init_f32(&cfft_inst, CZT_N + CZT_M - 1, 0, 1);
    if (status != ARM_MATH_SUCCESS) {
        return MY_CZT_ERROR_INIT_FAILED;
    }
    
    // 预计算chirp序列
    // 计算 chirp 序列：W^(n^2/2) for n = 0, 1, ..., N+M-2
    for (int n = 0; n < CZT_N + CZT_M - 1; n++) {
        float32_t angle = (n * n) * 0.5f * CZT_PHI;
        chirp_sequence_real[n] = arm_cos_f32(angle);
        chirp_sequence_imag[n] = arm_sin_f32(angle);
    }
    
    // 计算 chirp 序列的逆：W^(-k^2/2) for k = 0, 1, ..., M-1
    for (int k = 0; k < CZT_M; k++) {
        float32_t angle = -(k * k) * 0.5f * CZT_PHI;
        chirp_sequence_inv_real[k] = arm_cos_f32(angle);
        chirp_sequence_inv_imag[k] = arm_sin_f32(angle);
    }
    
    return MY_CZT_SUCCESS;
}

/**
 * @brief 执行CZT核心处理流程
 * @details 包括预计算、卷积计算等步骤
 * @param[in] input_real 输入信号实部数组
 * @param[in] input_imag 输入信号虚部数组
 * @param[out] output_real 输出信号实部数组
 * @param[out] output_imag 输出信号虚部数组
 * @return 0表示成功，其他值表示失败
 */
int my_czt_process(const float32_t* input_real, const float32_t* input_imag, 
                   float32_t* output_real, float32_t* output_imag)
{
    // 检查输入参数
    if (input_real == NULL || input_imag == NULL || 
        output_real == NULL || output_imag == NULL) {
        return MY_CZT_ERROR_INVALID_PARAMETER;
    }
    
    // 1. 将输入信号复制到内部缓冲区
    memcpy(signal_in_real, input_real, sizeof(float32_t) * CZT_N);
    memcpy(signal_in_imag, input_imag, sizeof(float32_t) * CZT_N);
    
    // 2. 构造序列 y(n) = x(n) * W^(n^2/2)
    for (int n = 0; n < CZT_N; n++) {
        // 复数乘法: (a + jb) * (c + jd) = (ac - bd) + j(ad + bc)
        float32_t a = signal_in_real[n];
        float32_t b = signal_in_imag[n];
        float32_t c = chirp_sequence_real[n];
        float32_t d = chirp_sequence_imag[n];
        
        intermediate_real[n] = a * c - b * d;
        intermediate_imag[n] = a * d + b * c;
    }
    
    // 填充剩余部分为0
    for (int n = CZT_N; n < CZT_N + CZT_M - 1; n++) {
        intermediate_real[n] = 0.0f;
        intermediate_imag[n] = 0.0f;
    }
    
    // 将实部和虚部数据交替存储到FFT输入缓冲区
    for (int n = 0; n < CZT_N + CZT_M - 1; n++) {
        fft_input_buffer[2 * n] = intermediate_real[n];     // 实部
        fft_input_buffer[2 * n + 1] = intermediate_imag[n]; // 虚部
    }
    
    // 3. FFT变换
    arm_cfft_radix4_f32(&cfft_inst, fft_input_buffer);  // 正向FFT
    
    // 将FFT输出结果拆分回实部和虚部数组
    for (int n = 0; n < CZT_N + CZT_M - 1; n++) {
        intermediate_real[n] = fft_input_buffer[2 * n];     // 实部
        intermediate_imag[n] = fft_input_buffer[2 * n + 1]; // 虚部
    }
    
    // 4. 点乘运算 (与chirp序列的逆进行点乘)
    for (int k = 0; k < CZT_M; k++) {
        // 复数乘法: (a + jb) * (c + jd) = (ac - bd) + j(ad + bc)
        float32_t a = intermediate_real[k];
        float32_t b = intermediate_imag[k];
        float32_t c = chirp_sequence_inv_real[k];
        float32_t d = chirp_sequence_inv_imag[k];
        
        intermediate_real[k] = a * c - b * d;
        intermediate_imag[k] = a * d + b * c;
    }
    
    // 填充剩余部分为0
    for (int k = CZT_M; k < CZT_N + CZT_M - 1; k++) {
        intermediate_real[k] = 0.0f;
        intermediate_imag[k] = 0.0f;
    }
    
    // 将实部和虚部数据交替存储到FFT输入缓冲区
    for (int n = 0; n < CZT_N + CZT_M - 1; n++) {
        fft_input_buffer[2 * n] = intermediate_real[n];     // 实部
        fft_input_buffer[2 * n + 1] = intermediate_imag[n]; // 虚部
    }
    
    // 5. IFFT变换
    arm_cfft_radix4_f32(&cfft_inst, fft_input_buffer);  // 反向FFT
    
    // 将FFT输出结果拆分回实部和虚部数组
    for (int n = 0; n < CZT_N + CZT_M - 1; n++) {
        intermediate_real[n] = fft_input_buffer[2 * n];     // 实部
        intermediate_imag[n] = fft_input_buffer[2 * n + 1]; // 虚部
    }
    
    // 6. 后处理并提取结果
    // 提取结果并乘以 W^(k^2/2) 得到最终的CZT结果
    for (int k = 0; k < CZT_M; k++) {
        // 复数乘法: (a + jb) * (c + jd) = (ac - bd) + j(ad + bc)
        float32_t a = intermediate_real[k] / (CZT_N + CZT_M - 1);  // 归一化
        float32_t b = intermediate_imag[k] / (CZT_N + CZT_M - 1);  // 归一化
        float32_t c = chirp_sequence_real[k];
        float32_t d = chirp_sequence_imag[k];
        
        signal_out_real[k] = a * c - b * d;
        signal_out_imag[k] = a * d + b * c;
    }
    
    // 7. 将结果复制到输出缓冲区
    memcpy(output_real, signal_out_real, sizeof(float32_t) * CZT_M);
    memcpy(output_imag, signal_out_imag, sizeof(float32_t) * CZT_M);
    
    return MY_CZT_SUCCESS;
}

/**
 * @brief 获取输入信号
 * @param[out] buffer_real 实部缓冲区
 * @param[out] buffer_imag 虚部缓冲区
 * @param[in] length 缓冲区长度
 */
void my_czt_get_input_signal(float32_t* buffer_real, float32_t* buffer_imag, size_t length)
{
    // 检查输入参数
    if (buffer_real == NULL || buffer_imag == NULL || length > CZT_N) {
        return;
    }
    // 复制数据到输出缓冲区
    memcpy(buffer_real, signal_in_real, sizeof(float32_t) * length);
    memcpy(buffer_imag, signal_in_imag, sizeof(float32_t) * length);
}

/**
 * @brief 获取输出信号
 * @param[out] buffer_real 实部缓冲区
 * @param[out] buffer_imag 虚部缓冲区
 * @param[in] length 缓冲区长度
 */
void my_czt_get_output_signal(float32_t* buffer_real, float32_t* buffer_imag, size_t length)
{
    // 检查输入参数
    if (buffer_real == NULL || buffer_imag == NULL || length > CZT_M) {
        return;
    }
    // 复制数据到输出缓冲区
    memcpy(buffer_real, signal_out_real, sizeof(float32_t) * length);
    memcpy(buffer_imag, signal_out_imag, sizeof(float32_t) * length);
}
#ifndef MY_CZT_CONFIG_H
#define MY_CZT_CONFIG_H

#include "arm_math.h"

// --- 核心参数 ---
/** @brief 输入序列长度 */
#define CZT_N                   1024

/** @brief 输出点数 */
#define CZT_M                   1024

/** @brief 起始点的幅度 R0 */
#define CZT_R0                  1.0f

/** @brief 起始点的相位 φ0 (弧度) */
#define CZT_PHI0                0.0f

/** @brief 复数旋转因子的幅度 R */
#define CZT_R                   1.0f

/** @brief 复数旋转因子的相位 φ (弧度) */
#define CZT_PHI                 (2.0f * PI / CZT_M)

/** @brief 起始点 A = R0 * exp(j*φ0) */
#define CZT_A_REAL              (CZT_R0 * arm_cos_f32(CZT_PHI0))
#define CZT_A_IMAG              (CZT_R0 * arm_sin_f32(CZT_PHI0))

/** @brief 复数旋转因子 W = R * exp(j*φ) */
#define CZT_W_REAL              (CZT_R * arm_cos_f32(CZT_PHI))
#define CZT_W_IMAG              (CZT_R * arm_sin_f32(CZT_PHI))

// --- 错误码定义 ---
typedef enum {
    MY_CZT_SUCCESS = 0,
    MY_CZT_ERROR_INVALID_PARAMETER,
    MY_CZT_ERROR_INIT_FAILED,
    MY_CZT_ERROR_PROCESS_FAILED,
    MY_CZT_ERROR_MEMORY_ALLOCATION
} my_czt_error_t;

// --- 函数声明 ---
/**
 * @brief 初始化CZT模块
 * @details 初始化FFT实例和相关参数
 * @return 0表示成功，其他值表示失败
 */
int my_czt_init(void);

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
                   float32_t* output_real, float32_t* output_imag);

/**
 * @brief 获取输入信号
 * @param[out] buffer_real 实部缓冲区
 * @param[out] buffer_imag 虚部缓冲区
 * @param[in] length 缓冲区长度
 */
void my_czt_get_input_signal(float32_t* buffer_real, float32_t* buffer_imag, size_t length);

/**
 * @brief 获取输出信号
 * @param[out] buffer_real 实部缓冲区
 * @param[out] buffer_imag 虚部缓冲区
 * @param[in] length 缓冲区长度
 */
void my_czt_get_output_signal(float32_t* buffer_real, float32_t* buffer_imag, size_t length);

#endif // MY_CZT_CONFIG_H
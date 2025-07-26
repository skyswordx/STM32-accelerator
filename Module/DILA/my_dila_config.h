#ifndef MY_DILA_CONFIG_H
#define MY_DILA_CONFIG_H

#include "arm_math.h"
#include "math.h"

// --- 核心参数 ---
/** @brief ADC采样频率 (Hz) */
#define DILA_FS                  400000.0f

/** @brief 期望的输入信号频率 (Hz) */
#define DILA_FIN_DESIRED         20000.0f

/** @brief ADC采集点数 / 处理的数据块大小 */
#define DILA_N                   4096

/** @brief 实际使用的输入信号频率 */
#define DILA_FIN_ACTUAL          20000.0f

// --- 真实信号参数 (用于生成测试信号) ---
/** @brief 测试信号的幅度 (V) */
#define DILA_AIN                 1.0f

/** @brief 测试信号的初始相位 (度) */
#define DILA_PHI_IN_DEG          45.0f

/** @brief 测试信号的初始相位 (弧度) */
#define DILA_PHI_IN_RAD          (DILA_PHI_IN_DEG * PI / 180.0f)

/** @brief 测试信号的噪声幅度 */
#define DILA_NOISE_AMP           0.01f

// --- FIR滤波器参数 ---
/** @brief 整个DILA处理流程的总延迟（样本数），用于截取稳态信号 */
#define DILA_TOTAL_DELAY    37

/** @brief 输入FIR滤波器的抽头数 */
#define INPUT_FIR_NUM_TAPS    10

/** @brief 输出FIR滤波器的抽头数 */
#define OUTPUT_FIR_NUM_TAPS    65

// --- 错误码定义 ---
typedef enum {
    MY_DILA_SUCCESS = 0,
    MY_DILA_ERROR_INVALID_PARAMETER,
    MY_DILA_ERROR_INIT_FAILED,
    MY_DILA_ERROR_PROCESS_FAILED
} my_dila_error_t;

// --- 函数声明 ---
/**
 * @brief 初始化DILA模块
 * @details 初始化FIR滤波器实例和相关参数
 * @param[in] input_fir_coeffs 输入FIR滤波器系数数组
 * @param[in] output_fir_coeffs 输出FIR滤波器系数数组
 * @return 0表示成功，其他值表示失败
 */
int my_dila_init(const float32_t* input_fir_coeffs, const float32_t* output_fir_coeffs);

/**
 * @brief 生成理想参考信号
 * @details 根据配置参数生成正交的正弦和余弦参考信号
 */
void my_dila_generate_reference_signals(void);

/**
 * @brief 执行DILA核心处理流程
 * @details 包括输入滤波、IQ解调、输出滤波等步骤
 * @param[in] input_signal 输入信号数组
 */
void my_dila_process_signal(const float32_t* input_signal);

/**
 * @brief 计算幅度和相位结果
 * @details 从滤波后的I/Q信号中计算幅度和相位
 * @param[out] magnitude 输出幅度值
 * @param[out] phase 输出相位值(弧度)
 * @param[out] phase_deg 输出相位值(度)
 */
void my_dila_calculate_results(float32_t* magnitude, float32_t* phase, float32_t* phase_deg);

/**
 * @brief 获取I通道滤波后的信号
 * @param[out] buffer 输出缓冲区
 * @param[in] length 缓冲区长度
 */
void my_dila_get_I_filtered(float32_t* buffer, size_t length);

/**
 * @brief 获取Q通道滤波后的信号
 * @param[out] buffer 输出缓冲区
 * @param[in] length 缓冲区长度
 */
void my_dila_get_Q_filtered(float32_t* buffer, size_t length);

#endif // MY_DILA_CONFIG_H

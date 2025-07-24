/**
 * @file dila_config.h
 * @brief Digital Lock-In Amplifier (DILA) 核心参数配置文件
 * @details
 * 定义了采样率、采样点数、信号频率等仿真和实现所需的全部参数。
 * 确保这些参数与MATLAB仿真中的设置严格一致。
 */

#ifndef DILA_CONFIG_H_
#define DILA_CONFIG_H_

#include "arm_math.h"
#include <math.h> // For M_PI

// --- 核心参数 (必须与MATLAB脚本一致) ---

/** @brief ADC采样频率 (Hz) */
#define DILA_FS                  400000.0f

/** @brief 期望的输入信号频率 (Hz) */
#define DILA_FIN_DESIRED         20000.0f

/** @brief ADC采集点数 / 处理的数据块大小 */
#define DILA_N                   4096

// --- 相干采样参数计算 (在C代码中重新计算以确保一致性) ---
// Fin = M * Fs / N
// M_float = Fin_desired * N / Fs
// M = round(M_float) -> if even, M++
// Fin_actual = M * Fs / N

// 在C代码中，我们将直接计算出M和实际的Fin，以生成参考信号
// #define M_FLOAT                  (DILA_FIN_DESIRED * DILA_N / DILA_FS)
// #define M_ROUNDED                (roundf(M_FLOAT))//这里写的比较复杂了，这里是想要做一个相干采样
// #define M_INTEGER                ( ( (int)M_ROUNDED % 2 == 0) ? (int)M_ROUNDED + 1 : (int)M_ROUNDED )
// #define DILA_FIN_ACTUAL          ( (float32_t)M_INTEGER * DILA_FS / DILA_N )//这里写的比较复杂了，这里是想要做一个奇数的相干采样，我算过20019.53 Hz，这里还是用固定的吧
#define DILA_FIN_ACTUAL          20000.0f //后面实际使用的输入信号频率

// --- 真实信号参数 (用于生成测试信号) ---
/** @brief 测试信号的幅度 (V) */
#define DILA_AIN                 1.0f

/** @brief 测试信号的初始相位 (度) */
#define DILA_PHI_IN_DEG          45.0f

/** @brief 测试信号的初始相位 (弧度) */
#define DILA_PHI_IN_RAD          (DILA_PHI_IN_DEG * PI / 180.0f)

/** @brief 测试信号的噪声幅度 */
#define DILA_NOISE_AMP           0.01f

#endif /* DILA_CONFIG_H_ */

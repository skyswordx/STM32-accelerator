#ifndef MY_QUINN_CONFIG_H
#define MY_QUINN_CONFIG_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "arm_math.h"
#include "my_freq_config.h"

// --- 核心参数 ---
/** @brief FFT点数 */
#define QUINN_FFT_LENGTH              FFT_LENGTH

/** @brief 使能Quinn算法标志 */
#define QUINN_ENABLE                  1

/** @brief Quinn算法结果的有效性阈值 */
#define QUINN_VALIDITY_THRESHOLD      0.5f

/** @brief Quinn算法结果的可信度阈值 */
#define QUINN_CONFIDENCE_THRESHOLD    0.8f

// --- 频率估计结果结构体 ---
typedef struct {
    float32_t frequency;           // 估计的频率值 (Hz)
    float32_t coarse_frequency;    // 粗略频率估计值 (Hz)
    float32_t delta;               // 频率偏移量
    float32_t confidence;          // 结果可信度 (0.0-1.0)
    uint8_t validity;              // 结果有效性标志 (1=有效, 0=无效)
    uint16_t peak_index;           // 峰值点索引
} quinn_frequency_result_t;

// --- 错误码定义 ---
typedef enum {
    MY_QUINN_SUCCESS = 0,
    MY_QUINN_ERROR_INVALID_PARAMETER,
    MY_QUINN_ERROR_PROCESS_FAILED,
    MY_QUINN_ERROR_INVALID_PEAK_INDEX
} my_quinn_error_t;

// --- 全局变量声明 ---
/** @brief Quinn模块运行时启用标志 */
extern uint8_t g_quinn_module_enabled;

// --- 函数声明 ---
/**
 * @brief 执行Quinn频率估计算法
 * @details 基于FFT结果应用Quinn算法进行精细频率估计
 * @param[in] fft_data FFT复数结果数组
 * @param[in] peak_index 检测到的峰值点索引
 * @param[out] result 频率估计结果结构体指针
 * @return 0表示成功，其他值表示失败
 */
int my_quinn_process(const float32_t* fft_data, uint16_t peak_index, quinn_frequency_result_t* result);

/**
 * @brief 获取Quinn算法的估计结果
 * @param[out] result 频率估计结果结构体指针
 */
void my_quinn_get_result(quinn_frequency_result_t* result);

/**
 * @brief 检查Quinn算法结果是否有效
 * @return 1表示有效，0表示无效
 */
uint8_t my_quinn_is_result_valid(void);

/**
 * @brief 获取Quinn算法结果的可信度
 * @return 可信度值 (0.0-1.0)
 */
float32_t my_quinn_get_confidence(void);

#endif // MY_QUINN_CONFIG_H
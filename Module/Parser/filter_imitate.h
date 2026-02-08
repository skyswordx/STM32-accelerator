#ifndef FILTER_IMITATE_H
#define FILTER_IMITATE_H

#include "arm_math.h"
#include "filter_identification.h"
#include "my_freq_config.h"

// 确保FFT_LENGTH已定义
#ifndef FFT_LENGTH
#define FFT_LENGTH (4096)
#endif

// 滤波器模仿状态
typedef enum {
    IMITATE_STATE_IDLE = 0,
    IMITATE_STATE_READY = 1,     // 已学习完成，准备模仿
    IMITATE_STATE_ACTIVE = 2     // 正在模仿输出
} filter_imitate_state_t;

// 滤波器模仿模块初始化
void filter_imitate_init(void);

// 设置学习到的传递函数
void filter_imitate_set_transfer_function(const ContinuousTransferFunction* tf);

// 处理输入信号并生成输出信号
// input_signal: 输入时域信号 (长度为FFT_LENGTH)
// output_signal: 输出时域信号 (长度为FFT_LENGTH)
// sampling_rate: 采样率 (Hz)
// 返回值: 0=成功, -1=失败
int filter_imitate_process_signal(const float32_t* input_signal, 
                                  float32_t* output_signal, 
                                  uint32_t sampling_rate);

// 获取当前模仿状态
filter_imitate_state_t filter_imitate_get_state(void);

// 重置模仿状态
void filter_imitate_reset(void);

#endif /* FILTER_IMITATE_H */

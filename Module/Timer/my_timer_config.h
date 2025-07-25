#ifndef MY_TIMER_CONFIG_H
#define MY_TIMER_CONFIG_H

#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"
#include "stdint.h"
#include "my_adc_task.h"

// 动态采样率调整的配置参数
//   4096   FFT点数
#define MIN_CYCLES_IN_FFT        2           // FFT窗口内最少周期数
#define MIN_POINTS_PER_CYCLE     20          // 每周期最少采样点数
#define COARSE_SAMPLE_RATE       250000      // 粗测初始采样率 (250kHz)
#define MAX_SIGNAL_FREQ          100000      // 最大信号频率 (100kHz)
#define MIN_SIGNAL_FREQ          1000        // 最小信号频率 (1kHz)



// 函数声明
void switch_timer_sampleRate_Manual(TIM_HandleTypeDef* htimer, uint32_t prescaler, uint32_t arr);
HAL_StatusTypeDef switch_timer_sampleRate_Auto(TIM_HandleTypeDef* htimer, uint32_t desired_sample_rate, uint32_t measured_freq);

// 自适应采样率调整函数
HAL_StatusTypeDef adaptive_set_sample_rate_Auto(TIM_HandleTypeDef* htimer, uint32_t measured_freq);
HAL_StatusTypeDef adaptive_set_sample_rate_Manual(TIM_HandleTypeDef* htimer, uint32_t measured_freq);
#endif /* MY_TIMER_CONFIG_H */

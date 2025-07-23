#ifndef MY_TIMER_CONFIG_H
#define MY_TIMER_CONFIG_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"

// 函数声明
void switch_timer_sampleRate_Manual(TIM_HandleTypeDef* htimer, uint32_t prescaler, uint32_t arr);
HAL_StatusTypeDef switch_timer_sampleRate_Auto(TIM_HandleTypeDef* htimer, uint32_t desired_sample_rate);

#endif /* MY_TIMER_CONFIG_H */

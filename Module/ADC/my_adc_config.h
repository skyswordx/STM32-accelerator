#ifndef MY_ADC_CONFIG_H
#define MY_ADC_CONFIG_H

#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */


#endif /* MY_ADC_CONFIG_H */
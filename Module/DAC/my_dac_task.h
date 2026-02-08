#ifndef MY_DAC_TASK_H
#define MY_DAC_TASK_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"
#include "my_dds.h"
#include "my_parameter_config.h"

// 外部变量声明
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim4;

// 全局变量声明
extern DDS_Generator_t g_dds_generator;

// 函数声明
void StartDACProcessingTask(void *argument);
void update_dac_waveform_by_parameters(void);
void set_dac_imitate_mode(const float32_t* output_waveform, uint32_t length);

#endif /* MY_DAC_TASK_H */

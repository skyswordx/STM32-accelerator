#ifndef MY_ZLCR_TASK_H
#define MY_ZLCR_TASK_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "arm_math.h"
#include "my_adc_task.h"
#include "my_freq_config.h"


typedef struct {
    uint32_t frequency;    // 频率(Hz)
    float32_t magnitude;   // 幅值(欧姆)
    float32_t phase;       // 相位(度)
} sweep_point_result_t;

extern sweep_point_result_t g_current_freq_result;

void my_zlcr_get_impedance(const fundamental_result_t *ch1_fundamental, const fundamental_result_t *ch2_fundamental, 
                           sweep_point_result_t *current_freq_result);



#endif // MY_ZLCR_TASK_H
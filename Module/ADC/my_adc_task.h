#ifndef MT_MY_ADC_TASK_H
#define MT_MY_ADC_TASK_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"

#include "my_timer_config.h"
#include "my_freq_config.h"
#include "my_zlcr_config.h"

// ADC工作模式枚举类型
typedef enum {
    ADC_MODE_IDLE = 0, 
    ADC_MODE_NORMAL = 1,     // 正常模式
    ADC_MODE_SWEEP = 2       // 扫频模式
} adc_mode_t;

extern uint32_t g_ADC_SAMPLE_RATE_Hz;

void StartADCProcessingTask(void *argument);

#endif /* MT_MY_ADC_TASK_H */
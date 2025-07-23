#ifndef MT_MY_ADC_TASK_H
#define MT_MY_ADC_TASK_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"

#include "my_timer_config.h"
#include "my_freq_config.h"

extern uint32_t g_ADC_SAMPLE_RATE_Hz;

void StartADCProcessingTask(void *argument);

#endif /* MT_MY_ADC_TASK_H */
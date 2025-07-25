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

void StartZLCRProcessingTask(void *argument);

#endif // MY_ZLCR_TASK_H
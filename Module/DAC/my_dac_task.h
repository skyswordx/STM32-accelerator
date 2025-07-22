#ifndef MY_DAC_TASK_H
#define MY_DAC_TASK_H

#include "main.h"
#include "my_dac_config.h"
#include "cmsis_os.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>


void StartDACProcessingTask(void *argument);

#endif /* MY_DAC_TASK_H */
#ifndef MY_BUTTON_TASK_H
#define MY_BUTTON_TASK_H


#include "stm32h7xx_hal.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"


// 按钮任务函数
void StartButtonProcessingTask(void *argument);

#endif // MY_BUTTON_TASK_H
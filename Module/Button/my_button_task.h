#ifndef MY_BUTTON_TASK_H
#define MY_BUTTON_TASK_H


#include "stm32h7xx_hal.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "../Param/my_parameter_config.h"  // 包含参数配置头文件

// 按钮任务函数
void StartButtonProcessingTask(void *argument);

// 按键解析相关函数
void reset_keypad_state(void);
void process_keypad_command(void);
void handle_set_command_keypad(char* param, uint32_t value);
void handle_get_command_keypad(char* param);

extern uint8_t g_short_pressed_key; // 短按按键
extern uint8_t g_long_pressed_key;  // 长按按键

#endif // MY_BUTTON_TASK_H
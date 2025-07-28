#ifndef MY_BUTTON_CONFIG_H
#define MY_BUTTON_CONFIG_H

#include "main.h"
#include "stdio.h"

// 全局变量声明
extern uint8_t g_short_pressed_key;
extern uint8_t g_long_pressed_key;
/* 虽然叫做 Button 其实是矩阵键盘*/


//================================================================================
// 1. 用户配置区 (User Configuration)
//================================================================================

// 定义行数和列数
#define KEYPAD_NUM_ROWS         3
#define KEYPAD_NUM_COLS         3

// 定义IO端口和引脚
// 根据电路图:
// Rows (Lines - Outputs)
// KEY_L1 -> PE0
// KEY_L2 -> PE2
// KEY_L3 -> PE4
#define KEYPAD_ROW1_PORT        GPIOE
#define KEYPAD_ROW1_PIN         GPIO_PIN_0
#define KEYPAD_ROW2_PORT        GPIOE
#define KEYPAD_ROW2_PIN         GPIO_PIN_2
#define KEYPAD_ROW3_PORT        GPIOE
#define KEYPAD_ROW3_PIN         GPIO_PIN_4

// Columns (Inputs)
// KEY_C1 -> PE1
// KEY_C2 -> PE3
// KEY_C3 -> PE5
#define KEYPAD_COL1_PORT        GPIOE
#define KEYPAD_COL1_PIN         GPIO_PIN_1
#define KEYPAD_COL2_PORT        GPIOE
#define KEYPAD_COL2_PIN         GPIO_PIN_3
#define KEYPAD_COL3_PORT        GPIOE
#define KEYPAD_COL3_PIN         GPIO_PIN_5

// 定义没有按键按下的返回值
#define NO_KEY_PRESSED          0x00

// 定义按键消抖时间 (单位: 毫秒)
// 建议值: 15-25ms
#define KEYPAD_DEBOUNCE_TIME_MS 20
// 定义长按的阈值 (单位: 毫秒)
#define KEYPAD_LONG_PRESS_THRESHOLD_MS 1000

//================================================================================
// 2. 函数声明 (Function Prototypes)
//================================================================================

/**
 * @brief  初始化矩阵键盘所需的GPIO引脚
 * @param  None
 * @retval None
 */
void Matrix_Keypad_Init(void);

/**
 * @brief  扫描键盘以获取按键值
 * @note   此函数需要被周期性调用 (例如每 20ms 在一个任务中调用).
 * 它内置了消抖和单次触发逻辑.
 * @param  None
 * @retval uint8_t:
 * - 如果有新的按键被稳定按下，返回对应的键值 (1-9).
 * - 如果没有新的按键按下或按键仍处于抖动/长按状态，返回 NO_KEY_PRESSED.
 */
uint8_t Matrix_Keypad_Scan(void);



#endif /* MY_BUTTON_CONFIG_H */

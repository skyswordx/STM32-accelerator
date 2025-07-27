#include "my_button_task.h"
#include "my_button_config.h"

void StartButtonProcessingTask(void *argument) {

        // UART任务代码
    uint8_t pressed_key = 0;
    
    for (;;) {

        /* 矩阵键盘测试 */
        // 1. 调用扫描函数
        pressed_key = Matrix_Keypad_Scan();

        // 2. 检查是否有新的按键按下
        if (pressed_key != NO_KEY_PRESSED) {
            // 在这里处理按键事件
            // 例如：通过串口打印
            printf("Key Pressed: %d\r\n", pressed_key);

           
        }
        osDelay(20);
    }
}

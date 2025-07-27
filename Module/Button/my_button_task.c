#include "my_button_task.h"
#include "my_button_config.h"
#include "AD9954.h"
#include "AD9833.h"
#include "INA226.h"


// uint8_t g_buttons[KEYPAD_NUM_COLS * KEYPAD_NUM_ROWS] = {0}; // 按键状态数组 0 表示未按下，1表示按下

uint8_t pressed_key = NO_KEY_PRESSED; // 没有按键按下时为 NO_KEY_PRESSED (0x00)

void StartButtonProcessingTask(void *argument) {

    // INA226 
    INA226_init();
    
    // AD9954 DDS
    AD9954_Init();
    AD9954_Set_Fre(1000.0);
    AD9954_Set_Amp(16383);
    AD9954_Set_Phase(0);

    // AD9833 DDS
    AD9833_Init_GPIO();
    AD9833_WaveSeting(1000.0, 0, SIN_WAVE, 0); // 设置AD9833为正弦波，频率1kHz，初相位0
    AD9833_AmpSet(0x1FFF); // 设置AD9833幅度为最大值
    uint16_t delta_frequency = 1000; // 频率增量



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
        
        
        switch (pressed_key) {
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            case 4:
                break;
            case 5:
                break;
            case 6:
                break;
            case 7:
                break;
            case 8:
                break;
            case 9:
                break;
        }
        // 3. 延时一段时间，避免过于频繁的扫描
        osDelay(20); // 20ms 延时
    }
}

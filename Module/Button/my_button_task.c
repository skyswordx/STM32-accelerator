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
        
        
        if (pressed_key == 1) {
            // 如果按下的是按键1，执行特定操作
            float bus_voltage = INA226_GetBusV();
            float current = INA226_GetCurrent();
            float power = INA226_GetPower();

            printf("Current: %.2f A, Voltage: %.2f V, Power: %.2f W\n", current, bus_voltage, power);

        }else if (pressed_key == 2) {
            AD9954_Set_Fre(delta_frequency); // 设置频率为1kHz
            AD9833_WaveSeting(delta_frequency, 0, SIN_WAVE, 0); // 设置AD9833频率为1kHz
            printf("DDS Frequency set to: %lu Hz\n", delta_frequency);

            delta_frequency += 1000; // 每次增加1kHz
            if (delta_frequency > 10000000) { // 如果超过10MHz，重置为1kHz
                delta_frequency = 1000;
            }
        }
        osDelay(20);
    }
}

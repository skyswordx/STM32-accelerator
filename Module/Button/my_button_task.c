#include "my_button_task.h"
#include "my_button_config.h"
#include "AD9954.h"
#include "AD9833.h"
#include "INA226.h"
#include "my_zlcr_config.h"


// uint8_t g_buttons[KEYPAD_NUM_COLS * KEYPAD_NUM_ROWS] = {0}; // 按键状态数组 0 表示未按下，1表示按下

// uint8_t g_pressed_key = NO_KEY_PRESSED; // 没有按键按下时为 NO_KEY_PRESSED (0x00)
extern uint8_t g_short_pressed_key; // 短按按键
extern uint8_t g_long_pressed_key;  // 长按按键
extern uint32_t g_desired_dds_frequency; // 用户期望设置的DDS频率

static uint8_t relay_state = 0; // 继电器状态计数器

void StartButtonProcessingTask(void *argument) {

    // INA226 
    INA226_init();
    
    my_zlcr_dds_init(DDS_TYPE_AD9833);
    // AD9954 DDS
    // AD9954_Set_Fre(1000.0);
    // AD9954_Set_Amp(16383);
    // AD9954_Set_Phase(0);

    // AD9833 DDS
    // AD9833_WaveSeting(1000.0, 0, SIN_WAVE, 0); // 设置AD9833为正弦波，频率1kHz，初相位0
    // AD9833_AmpSet(0x1FFF); // 设置AD9833幅度为最大值
    uint32_t delta = 0;

    for (;;) {

        /* 矩阵键盘测试 */
        // 1. 调用扫描函数
        Matrix_Keypad_Scan();

        // 2. 检查是否有新的短按按键
        if (g_short_pressed_key != NO_KEY_PRESSED) {
            // 在这里处理短按按键事件
            // 例如：通过串口打印
            printf("Short Key Pressed: %d\r\n", g_short_pressed_key);
            
            switch (g_short_pressed_key) {
                case 1:
                    break;
                case 2:
                    // 按键2: AD9833 DDS 测试
                    AD9833_WaveSeting(g_desired_dds_frequency+ 100*delta, 0, SIN_WAVE, 0); // 设置AD9833为正弦波
                    AD9833_AmpSet(120); // 设置AD9833幅度为
                    printf("AD9833: Set Frequency to %lu Hz\r\n", g_desired_dds_frequency + 100*delta);
                    delta++;
                    break;
                case 3:
                    // 按键3: AD9954 DDS 测试
                    AD9954_Set_Fre(g_desired_dds_frequency+ 100*delta);
                    AD9954_Set_Amp(16383/2.0); // 设置AD9954幅度为
                    AD9954_Set_Phase(0);
                    printf("AD9954: Set Frequency to %lu Hz\r\n", g_desired_dds_frequency + 100*delta);
                    delta++;
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
            // 处理完后清除短按按键状态
            g_short_pressed_key = NO_KEY_PRESSED;
        }

        // 3. 检查是否有新的长按按键
        if (g_long_pressed_key != NO_KEY_PRESSED) {
            // 在这里处理长按按键事件
            // 例如：通过串口打印
            printf("Long Key Pressed: %d\r\n", g_long_pressed_key);
            // 可以在这里添加长按的特殊处理逻辑
            // 处理完后清除长按按键状态
            g_long_pressed_key = NO_KEY_PRESSED;
        }

        // 4. 延时一段时间，避免过于频繁的扫描
        osDelay(20); // 20ms 延时
    }
}

#include "my_button_task.h"
#include "my_button_config.h"
#include "AD9954.h"
#include "AD9833.h"
#include "INA226.h"
#include "my_zlcr_config.h"


// uint8_t g_buttons[KEYPAD_NUM_COLS * KEYPAD_NUM_ROWS] = {0}; // 按键状态数组 0 表示未按下，1表示按下

uint8_t g_pressed_key = NO_KEY_PRESSED; // 没有按键按下时为 NO_KEY_PRESSED (0x00)
uint32_t g_desired_dds_frequency = 1000; // 用户期望设置的DDS频率

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

    for (;;) {

        /* 矩阵键盘测试 */
        // 1. 调用扫描函数
        g_pressed_key = Matrix_Keypad_Scan();

        // 2. 检查是否有新的按键按下
        if (g_pressed_key != NO_KEY_PRESSED) {
            // 在这里处理按键事件
            // 例如：通过串口打印
            printf("Key Pressed: %d\r\n", g_pressed_key);
        }


        switch (g_pressed_key) {
            case 1:
                // 按键1: INA226 测试
                float32_t voltage = INA226_GetBusV();
                float32_t current = INA226_GetCurrent();
                float32_t power = INA226_GetPower();

                printf("INA226: Voltage: %.2f V, Current: %.2f A, Power: %.2f W\r\n", voltage, current, power);
                break;
            case 2:
                // 按键2: AD9833 DDS 测试
                AD9833_WaveSeting(g_desired_dds_frequency, 0, SIN_WAVE, 0); // 设置AD9833为正弦波
                AD9833_AmpSet(120); // 设置AD9833幅度为
                printf("AD9833: Set Frequency to %lu Hz\r\n", g_desired_dds_frequency);
                break;
            case 3:
                // 按键3: AD9954 DDS 测试
                AD9954_Set_Fre(g_desired_dds_frequency);
                AD9954_Set_Amp(16383/2.0); // 设置AD9954幅度为
                AD9954_Set_Phase(0);
                printf("AD9954: Set Frequency to %lu Hz\r\n", g_desired_dds_frequency);
                break;
            case 4:
                // 按键4: 继电器测试
                // 根据relay_state设置对应的GPIO引脚
                // 先重置所有引脚
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET); 
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); 
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
                switch(relay_state) {
                    case 0: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET); break;
                    case 1: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET); break;
                    case 2: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET); break;
                    case 3: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); break;
                }
                relay_state = (relay_state + 1) % 4; // 循环计数
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

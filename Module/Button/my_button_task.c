#include "my_button_task.h"
#include "my_button_config.h"
#include "AD9954.h"
#include "AD9833.h"
#include "INA226.h"
#include "my_zlcr_config.h"
#include "my_uart_task.h"  // 包含串口任务头文件以使用解析函数
#include "../Param/my_parameter_config.h"  // 包含参数配置头文件
#include <string.h>
#include <stdlib.h>

// uint8_t g_buttons[KEYPAD_NUM_COLS * KEYPAD_NUM_ROWS] = {0}; // 按键状态数组 0 表示未按下，1表示按下

// uint8_t g_pressed_key = NO_KEY_PRESSED; // 没有按键按下时为 NO_KEY_PRESSED (0x00)
extern uint8_t g_short_pressed_key; // 短按按键
extern uint8_t g_long_pressed_key;  // 长按按键
extern uint32_t g_desired_dds_frequency; // 用户期望设置的DDS频率

// 按键解析状态机相关定义
typedef enum {
    IDLE_STATE,              // 空闲状态，等待第一次按键
    WAIT_PARAM_STATE,        // 等待第二次按键（参数类型）
    WAIT_VALUE_STATE,        // 等待第三次按键（值）
    NUMBER_INPUT_STATE,      // 数字输入模式
    ERROR_STATE              // 错误状态
} keypad_state_t;

// 按键解析状态机变量
static keypad_state_t keypad_state = IDLE_STATE;
static uint8_t cmd_type = 0;     // 命令类型
static uint8_t param = 0;        // 参数类型
static uint32_t value = 0;       // 值
static uint32_t number_buffer = 0; // 数字输入缓冲区

static uint8_t relay_state = 0; // 继电器状态计数器

// 函数声明
void reset_keypad_state(void);
void process_keypad_command(void);
void handle_set_command_keypad(char* param, uint32_t value);
void handle_get_command_keypad(char* param);
void print_command_hint(uint8_t cmd_type);
void print_param_hint(uint8_t cmd_type, uint8_t param);
void print_keypad_instructions(void);

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

    // 打印矩阵键盘使用说明书
    print_keypad_instructions();

    for (;;) {

        /* 矩阵键盘测试 */
        // 1. 调用扫描函数
        Matrix_Keypad_Scan();

        // 2. 检查是否有新的短按按键
        if (g_short_pressed_key != NO_KEY_PRESSED) {
            // 在这里处理短按按键事件
            // 例如：通过串口打印
            printf("Short Key Pressed: %d\r\n", g_short_pressed_key);
            
            // 检查是否处于数字输入模式
            if (keypad_state == NUMBER_INPUT_STATE) {
                // 在数字输入模式下，按键1-9用于输入数字
                if (g_short_pressed_key >= 1 && g_short_pressed_key <= 9) {
                    // 将按键值添加到数字缓冲区
                    number_buffer = number_buffer * 10 + g_short_pressed_key;
                    printf("Number input: %lu\r\n", number_buffer);
                }
            } else {
                // 正常的命令解析流程
                switch (keypad_state) {
                    case IDLE_STATE:
                        // 第一次按键，表示命令类型
                        cmd_type = g_short_pressed_key;
                        print_command_hint(cmd_type);
                        keypad_state = WAIT_PARAM_STATE;
                        break;
                        
                    case WAIT_PARAM_STATE:
                        // 第二次按键，表示参数类型
                        param = g_short_pressed_key;
                        print_param_hint(cmd_type, param);
                        keypad_state = WAIT_VALUE_STATE;
                        break;
                        
                    case WAIT_VALUE_STATE:
                        // 第三次按键，表示值
                        value = g_short_pressed_key;
                        printf("Value set to: %lu\r\n", value);
                        // 处理命令
                        process_keypad_command();
                        // 重置状态机
                        reset_keypad_state();
                        break;
                        
                    default:
                        break;
                }
            }
            
            // 处理完后清除短按按键状态
            g_short_pressed_key = NO_KEY_PRESSED;
        }

        // 3. 检查是否有新的长按按键
        if (g_long_pressed_key != NO_KEY_PRESSED) {
            // 在这里处理长按按键事件
            // 例如：通过串口打印
            printf("Long Key Pressed: %d\r\n", g_long_pressed_key);
            
            // 特殊处理长按按键
            switch (g_long_pressed_key) {
                case 1:
                    // 长按1键，进入数字输入模式
                    if (keypad_state == WAIT_VALUE_STATE) {
                        keypad_state = NUMBER_INPUT_STATE;
                        number_buffer = 0; // 重置数字缓冲区
                        printf("Enter number input mode\r\n");
                    }
                    break;
                    
                case 9:
                    // 长按9键，取消当前输入
                    reset_keypad_state();
                    printf("Input cancelled\r\n");
                    break;
                    
                default:
                    // 其他长按按键的处理
                    if (keypad_state == NUMBER_INPUT_STATE) {
                        // 在数字输入模式下长按其他键，确认输入
                        value = number_buffer;
                        printf("Number input confirmed: %lu\r\n", value);
                        // 处理命令
                        process_keypad_command();
                        // 重置状态机
                        reset_keypad_state();
                    }
                    break;
            }
            
            // 处理完后清除长按按键状态
            g_long_pressed_key = NO_KEY_PRESSED;
        }

        // 4. 延时一段时间，避免过于频繁的扫描
        osDelay(20); // 20ms 延时
    }
}

/**
 * @brief 重置按键解析状态机
 */
void reset_keypad_state(void) {
    keypad_state = IDLE_STATE;
    cmd_type = 0;
    param = 0;
    value = 0;
    number_buffer = 0;
}

/**
 * @brief 处理按键命令
 */
void process_keypad_command(void) {
    // 根据命令类型处理命令
    switch (cmd_type) {
        case 1:
            // 命令类型1：设置参数命令 (类似SET)
            {
                char param_str[20];
                // 根据参数类型确定参数名
                switch (param) {
                    case 1: strcpy(param_str, "DDS_FREQ"); break;
                    case 2: strcpy(param_str, "ADC_RATE"); break;
                    case 3: strcpy(param_str, "DDS_TYPE"); break;
                    case 4: strcpy(param_str, "DDS_PHASE"); break;
                    case 5: strcpy(param_str, "DDS_AMP"); break;
                    case 6: strcpy(param_str, "DAC_WAVE"); break;
                    case 7: strcpy(param_str, "DAC_FREQ"); break;
                    case 8: strcpy(param_str, "DAC_AMP"); break;
                    case 9: strcpy(param_str, "RELAY"); break;
                    default: strcpy(param_str, "UNKNOWN"); break;
                }
                handle_set_command_keypad(param_str, value);
            }
            break;
            
        case 2:
            // 命令类型2：读取参数命令 (类似GET)
            {
                char param_str[20];
                // 根据参数类型确定参数名
                switch (param) {
                    case 1: strcpy(param_str, "DDS_FREQ"); break;
                    case 2: strcpy(param_str, "ADC_RATE"); break;
                    case 3: strcpy(param_str, "ALL"); break; // 读取所有参数
                    default: strcpy(param_str, "UNKNOWN"); break;
                }
                handle_get_command_keypad(param_str);
            }
            break;
            
        default:
            // 其他命令类型可以在这里添加
            printf("Unknown command type: %d\r\n", cmd_type);
            break;
    }
}

/**
 * @brief 处理按键SET命令
 * @param param 参数名
 * @param value 参数值
 */
void handle_set_command_keypad(char* param, uint32_t value) {
    if (strcmp(param, "DDS_FREQ") == 0) {
        g_desired_dds_frequency = value;
        printf("Set DDS frequency: %lu Hz\r\n", g_desired_dds_frequency);
        // 更新DDS频率
        // 注意：这里需要根据实际的DDS类型来调用相应的函数
        // 暂时假设使用AD9833
        AD9833_WaveSeting(g_desired_dds_frequency, 0, SIN_WAVE, 0);
    }
    else if (strcmp(param, "ADC_RATE") == 0) {
        // 这里需要访问ADC采样率变量，但可能在其他文件中定义
        printf("Set ADC sample rate: %lu Hz\r\n", value);
    }
    else if (strcmp(param, "DDS_TYPE") == 0) {
        printf("Set DDS type: %lu\r\n", value);
    }
    else if (strcmp(param, "DDS_PHASE") == 0) {
        printf("Set DDS phase: %lu degrees\r\n", value);
    }
    else if (strcmp(param, "DDS_AMP") == 0) {
        printf("Set DDS amplitude: %lu\r\n", value);
    }
    else if (strcmp(param, "DAC_WAVE") == 0) {
        printf("Set DAC waveform: %lu\r\n", value);
    }
    else if (strcmp(param, "DAC_FREQ") == 0) {
        printf("Set DAC frequency: %lu Hz\r\n", value);
    }
    else if (strcmp(param, "DAC_AMP") == 0) {
        printf("Set DAC amplitude: %lu\r\n", value);
    }
    else if (strcmp(param, "RELAY") == 0) {
        printf("Set relay: %lu\r\n", value);
    }
    else {
        printf("Unknown parameter: %s\r\n", param);
    }
}

/**
 * @brief 处理按键GET命令
 * @param param 参数名
 */
void handle_get_command_keypad(char* param) {
    if (strcmp(param, "ALL") == 0) {
        printf("=== System Parameters ===\r\n");
        printf("DDS Frequency: %lu Hz\r\n", g_desired_dds_frequency);
        // 其他参数的打印...
    }
    else if (strcmp(param, "DDS_FREQ") == 0) {
        printf("DDS Frequency: %lu Hz\r\n", g_desired_dds_frequency);
    }
    else if (strcmp(param, "ADC_RATE") == 0) {
        printf("ADC Sample Rate: %lu Hz\r\n", 2000000UL); // 默认值
    }
    else {
        printf("Unknown parameter: %s\r\n", param);
    }
}

/**
 * @brief 打印命令类型提示信息
 * @param cmd_type 命令类型
 */
void print_command_hint(uint8_t cmd_type) {
    switch (cmd_type) {
        case 1:
            printf("Command: SET (Set parameter)\r\n");
            printf("Please press key for parameter type:\r\n");
            printf("  Key 1: DDS Frequency\r\n");
            printf("  Key 2: ADC Sample Rate\r\n");
            printf("  Key 3: DDS Type\r\n");
            printf("  Key 4: DDS Phase\r\n");
            printf("  Key 5: DDS Amplitude\r\n");
            printf("  Key 6: DAC Waveform\r\n");
            printf("  Key 7: DAC Frequency\r\n");
            printf("  Key 8: DAC Amplitude\r\n");
            printf("  Key 9: Relay Control\r\n");
            break;
        case 2:
            printf("Command: GET (Get parameter)\r\n");
            printf("Please press key for parameter type:\r\n");
            printf("  Key 1: DDS Frequency\r\n");
            printf("  Key 2: ADC Sample Rate\r\n");
            printf("  Key 3: All Parameters\r\n");
            break;
        default:
            printf("Unknown command type: %d\r\n", cmd_type);
            printf("Please press key 1 for SET or key 2 for GET\r\n");
            break;
    }
}

/**
 * @brief 打印参数类型提示信息
 * @param cmd_type 命令类型
 * @param param 参数类型
 */
void print_param_hint(uint8_t cmd_type, uint8_t param) {
    if (cmd_type == 1) { // SET命令
        switch (param) {
            case 1:
                printf("Parameter: DDS Frequency\r\n");
                printf("Please enter frequency value (Hz)\r\n");
                printf("Press key 1-9 to input digits, long press key 1 to enter number input mode\r\n");
                break;
            case 2:
                printf("Parameter: ADC Sample Rate\r\n");
                printf("Please enter sample rate value (Hz)\r\n");
                printf("Press key 1-9 to input digits, long press key 1 to enter number input mode\r\n");
                break;
            case 3:
                printf("Parameter: DDS Type\r\n");
                printf("Please enter DDS type:\r\n");
                printf("  Key 1: AD9833\r\n");
                printf("  Key 2: AD9954\r\n");
                break;
            case 4:
                printf("Parameter: DDS Phase\r\n");
                printf("Please enter phase value (degrees)\r\n");
                printf("Press key 1-9 to input digits, long press key 1 to enter number input mode\r\n");
                break;
            case 5:
                printf("Parameter: DDS Amplitude\r\n");
                printf("Please enter amplitude value\r\n");
                printf("Press key 1-9 to input digits, long press key 1 to enter number input mode\r\n");
                break;
            case 6:
                printf("Parameter: DAC Waveform\r\n");
                printf("Please enter waveform type:\r\n");
                printf("  Key 1: Sine\r\n");
                printf("  Key 2: Cosine\r\n");
                printf("  Key 3: Square\r\n");
                printf("  Key 4: Triangle\r\n");
                break;
            case 7:
                printf("Parameter: DAC Frequency\r\n");
                printf("Please enter frequency value (Hz)\r\n");
                printf("Press key 1-9 to input digits, long press key 1 to enter number input mode\r\n");
                break;
            case 8:
                printf("Parameter: DAC Amplitude\r\n");
                printf("Please enter amplitude value\r\n");
                printf("Press key 1-9 to input digits, long press key 1 to enter number input mode\r\n");
                break;
            case 9:
                printf("Parameter: Relay Control\r\n");
                printf("Please enter relay number (1-4)\r\n");
                break;
            default:
                printf("Unknown parameter type: %d\r\n", param);
                break;
        }
    } else if (cmd_type == 2) { // GET命令
        switch (param) {
            case 1:
                printf("Parameter: DDS Frequency\r\n");
                printf("Press any key to confirm reading\r\n");
                break;
            case 2:
                printf("Parameter: ADC Sample Rate\r\n");
                printf("Press any key to confirm reading\r\n");
                break;
            case 3:
                printf("Parameter: All Parameters\r\n");
                printf("Press any key to confirm reading\r\n");
                break;
            default:
                printf("Unknown parameter type: %d\r\n", param);
                break;
        }
    } else {
        printf("Unknown command type: %d\r\n", cmd_type);
    }
}



/**
 * @brief 打印矩阵键盘使用说明书
 */
void print_keypad_instructions(void) {
    printf("===========================================\r\n");
    printf("     Matrix Keypad Usage Instructions     \r\n");
    printf("===========================================\r\n");
    printf("\r\n");
    printf("1. Command Structure:\r\n");
    printf("   [Command Type] -> [Parameter Type] -> [Value]\r\n");
    printf("\r\n");
    printf("2. Command Types:\r\n");
    printf("   Key 1: SET (Set parameter)\r\n");
    printf("   Key 2: GET (Get parameter)\r\n");
    printf("\r\n");
    printf("3. SET Command Parameters:\r\n");
    printf("   Key 1: DDS Frequency (Hz)\r\n");
    printf("   Key 2: ADC Sample Rate (Hz)\r\n");
    printf("   Key 3: DDS Type (1=AD9833, 2=AD9954)\r\n");
    printf("   Key 4: DDS Phase (degrees)\r\n");
    printf("   Key 5: DDS Amplitude\r\n");
    printf("   Key 6: DAC Waveform (1=Sine, 2=Cosine, 3=Square, 4=Triangle)\r\n");
    printf("   Key 7: DAC Frequency (Hz)\r\n");
    printf("   Key 8: DAC Amplitude\r\n");
    printf("   Key 9: Relay Control (1-4)\r\n");
    printf("\r\n");
    printf("4. GET Command Parameters:\r\n");
    printf("   Key 1: DDS Frequency\r\n");
    printf("   Key 2: ADC Sample Rate\r\n");
    printf("   Key 3: All Parameters\r\n");
    printf("\r\n");
    printf("5. Value Input:\r\n");
    printf("   - For numeric values, press keys 1-9 to input digits\r\n");
    printf("   - Long press key 1 to enter number input mode for multi-digit numbers\r\n");
    printf("   - Long press key 9 to cancel input\r\n");
    printf("\r\n");
    printf("6. Examples:\r\n");
    printf("   - Set DDS frequency to 1000 Hz: Press 1, 1, 1000, Enter\r\n");
    printf("   - Get DDS frequency: Press 2, 1, Any key\r\n");
    printf("   - Set DAC waveform to sine: Press 1, 6, 1, Enter\r\n");
    printf("\r\n");
    printf("===========================================\r\n");
}

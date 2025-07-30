#include "my_parser.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"
#include "AD9833.h"
#include "AD9954.h"
#include "my_timer_config.h"
#include "my_button_config.h"


extern TIM_HandleTypeDef htim4;
extern DAC_HandleTypeDef hdac1;

extern uint8_t g_short_pressed_key; // 短按按键
extern uint8_t g_long_pressed_key;  // 长按按键

// 按键状态跟踪变量
uint8_t key1_pressed_count = 0;  // 按键1按下的次数
uint8_t key2_pressed_count = 0;  // 按键2按下的次数
uint8_t key3_pressed_count = 0;  // 按键3按下的次数


// 函数模式跟踪变量
uint8_t current_function_mode = FUNCTION_MODE_NONE;  // 当前函数模式


// base2_function模式下的幅度跟踪变量
uint8_t base2_ad9833_amplitude = 255;  // AD9833幅度初始值
uint16_t base2_ad9954_amplitude = 16383;  // AD9954幅度初始值
uint16_t base4_ad9954_amplitude = 16383;  // AD9954幅度初始值
double base2_current_frequency = 1000.0;  // 当前频率，初始为1kHz

// base3_function模式下的幅度跟踪变量
uint8_t base3_ad9833_amplitude = 255;  // AD9833幅度初始值
uint16_t base3_ad9954_amplitude = 16383;  // AD9954幅度初始值

// base4_function模式下表格输入
/**
 * 第一个维度行是 100Hz, 200 Hz, ..., 3000 Hz 一共 30 行
 * 第二个维度列是 1.0, 1.1, ..., 1.9 ,2.0 一共 11 列
 * 每个元素是一个 double 类型的值，表示 DDS 在这种情况下要输出的幅度
 */
double base4_ad9954_table[30][11] = {0};


void myParserTask(void const * argument)
{
    // Initialize the parser
    AD9833_Init_GPIO();
    AD9954_Init(); // Initialize AD9954
    // Set amplitude to 2V for AD9833 using the voltage conversion macro
    AD9833_AmpSet(255/2);
    
    // Set amplitude to maximum for AD9954 (max value is 16383)
    AD9954_Set_Amp(16383/2);
    AD9954_Set_Phase(0);//写相位

    
    while (1) {
        // 1. 调用扫描函数
        Matrix_Keypad_Scan();

        // 2. 检查是否有新的短按按键
        if (g_short_pressed_key != NO_KEY_PRESSED) {
            // 在这里处理短按按键事件
            // 例如：通过串口打印
            printf("Short Key Pressed: %d\r\n", g_short_pressed_key);
            
            switch (g_short_pressed_key) {
                case 1:
                    // 按键1在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_NONE:
                            // 在无模式下，第一次按下按键1进入base2_function
                            key1_pressed_count++;
                            if (key1_pressed_count == 1) {
                                printf("First press of key 1, entering base2_function\n");
                                base2_function();
                            }
                            break;
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键1可以有其他功能
                            printf("Key 1 pressed in base2_function mode: Custom function for base2\n");
                            // 在base2_function模式下，按键1无特殊功能
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键1可以有其他功能
                            printf("Key 1 pressed in base3_function mode: Custom function for base3\n");
                            // 在这里添加base3_function模式下按键1的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键1可以有其他功能
                            printf("Key 1 pressed in base4_function mode: Custom function for base4\n");
                            // 在这里添加base4_function模式下按键1的特殊功能
                            break;
                        default:
                            printf("Key 1 pressed\n");
                            break;
                    }
                    break;
                case 2:
                    // 按键2在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_NONE:
                            // 在无模式下，第一次按下按键2进入base3_function
                            key2_pressed_count++;
                            if (key2_pressed_count == 1) {
                                printf("First press of key 2, entering base3_function\n");
                                base3_function();
                            }
                            break;
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键2控制所有DDS的幅度增加
                            printf("Key 2 pressed in base2_function mode: Increasing all DDS amplitude\n");
                            // 增加AD9833幅度
                            // 将当前DAC值转换回电压值
                            float current_ad9833_voltage = (float)base2_ad9833_amplitude * AD9833_VMAX_V / 255.0;
                            // 增加0.1V
                            current_ad9833_voltage += 0.1;
                            // 检查是否超过最大电压
                            if (current_ad9833_voltage > AD9833_VMAX_V) {
                                current_ad9833_voltage = AD9833_VMAX_V;
                            }
                            // 转换为DAC值
                            base2_ad9833_amplitude = AD9833_VOLTAGE_TO_DAC(current_ad9833_voltage);
                            AD9833_AmpSet(base2_ad9833_amplitude);
                            
                            // 增加AD9954幅度
                            // 将当前DAC值转换回电压值
                            float current_ad9954_voltage = (float)base2_ad9954_amplitude * AD9954_VMAX_V / 16383.0;
                            // 增加0.1V
                            current_ad9954_voltage += 0.1;
                            // 检查是否超过最大电压
                            if (current_ad9954_voltage > AD9954_VMAX_V) {
                                current_ad9954_voltage = AD9954_VMAX_V;
                            }
                            // 转换为DAC值
                            base2_ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(current_ad9954_voltage);
                            AD9954_Set_Amp(base2_ad9954_amplitude);
                            
                            printf("New amplitudes - AD9833: %d, AD9954: %d\n", base2_ad9833_amplitude, base2_ad9954_amplitude);
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键2可以有其他功能
                            printf("Key 2 pressed in base3_function mode: Custom function for base3\n");
                            // 在这里添加base3_function模式下按键2的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键2可以有其他功能
                            printf("Key 2 pressed in base4_function mode: Custom function for base4\n");
                            // 在这里添加base4_function模式下按键2的特殊功能
                            break;
                        default:
                            printf("Key 2 pressed\n");
                            break;
                    }
                    break;
                case 3:
                    // 按键3在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_NONE:
                            // 在无模式下，第一次按下按键3进入base4_function
                            key3_pressed_count++;
                            if (key3_pressed_count == 1) {
                                printf("First press of key 3, entering base4_function\n");
                                base4_function();
                            }
                            break;
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键3无特殊功能
                            printf("Key 3 pressed in base2_function mode: No function\n");
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键3可以有其他功能
                            printf("Key 3 pressed in base3_function mode: Custom function for base3\n");
                            // 在这里添加base3_function模式下按键3的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键3可以有其他功能
                            printf("Key 3 pressed in base4_function mode: Custom function for base4\n");
                            // 在这里添加base4_function模式下按键3的特殊功能
                            break;
                        default:
                            printf("Key 3 pressed\n");
                            break;
                    }
                    break;
                case 4:
                    // 按键4在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键4控制所有DDS的频率减小100Hz
                            printf("Key 4 pressed in base2_function mode: Decreasing all DDS frequencies by 100Hz\n");
                            // 减小频率100Hz，但不低于1kHz
                            if (base2_current_frequency > 1100.0) {
                                base2_current_frequency -= 100.0;
                            } else {
                                base2_current_frequency = 1000.0;
                            }
                            // 设置AD9833频率
                            AD9833_WaveSeting(base2_current_frequency, 0, SIN_WAVE, 0);
                            // 设置AD9954频率
                            AD9954_Set_Fre(base2_current_frequency);
                            printf("New frequency: %.0f Hz\n", base2_current_frequency);
                            break;
                        case FUNCTION_MODE_BASE3:
      
                            break;
                        case FUNCTION_MODE_BASE4:

                            break;
                        default:
                            printf("Key 4 pressed\n");
                            break;
                    }
                    break;
                case 5:
                    // 按键5在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键5可以有特殊功能
                            printf("Key 5 pressed in base2_function mode: Special function for base2\n");
                            // 在这里添加base2_function模式下按键5的特殊功能
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键5可以有特殊功能
                            printf("Key 5 pressed in base3_function mode: Special function for base3\n");
                            // 在这里添加base3_function模式下按键5的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键5可以有特殊功能
                            printf("Key 5 pressed in base4_function mode: Special function for base4\n");
                            // 在这里添加base4_function模式下按键5的特殊功能
                            break;
                        default:
                            printf("Key 5 pressed\n");
                            break;
                    }
                    break;
                case 6:
                    // 按键6在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键6控制所有DDS的频率增加100Hz
                            printf("Key 6 pressed in base2_function mode: Increasing all DDS frequencies by 100Hz\n");
                            // 增加频率100Hz，但不高于1MHz
                            if (base2_current_frequency < 999900.0) {
                                base2_current_frequency += 100.0;
                            } else {
                                base2_current_frequency = 1000000.0;
                            }
                            // 设置AD9833频率
                            AD9833_WaveSeting(base2_current_frequency, 0, SIN_WAVE, 0);
                            // 设置AD9954频率
                            AD9954_Set_Fre(base2_current_frequency);
                            printf("New frequency: %.0f Hz\n", base2_current_frequency);
                            break;
                        case FUNCTION_MODE_BASE3:

                            break;
                        case FUNCTION_MODE_BASE4:

                            break;
                        default:
                            printf("Key 6 pressed\n");
                            break;
                    }
                    break;
                case 7:
                    // 按键7在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键7可以有特殊功能
                            printf("Key 7 pressed in base2_function mode: Special function for base2\n");
                            // 在这里添加base2_function模式下按键7的特殊功能
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键7可以有特殊功能
                            printf("Key 7 pressed in base3_function mode: Special function for base3\n");
                            // 在这里添加base3_function模式下按键7的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键7可以有特殊功能
                            printf("Key 7 pressed in base4_function mode: Special function for base4\n");
                            // 在这里添加base4_function模式下按键7的特殊功能
                            break;
                        default:
                            printf("Key 7 pressed\n");
                            break;
                    }
                    break;
                case 8:
                    // 按键8在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键8控制所有DDS的幅度减小
                            printf("Key 8 pressed in base2_function mode: Decreasing all DDS amplitude\n");
                            // 减小AD9833幅度
                            // 将当前DAC值转换回电压值
                            float current_ad9833_voltage = (float)base2_ad9833_amplitude * AD9833_VMAX_V / 255.0;
                            // 减小0.1V
                            current_ad9833_voltage -= 0.1;
                            // 检查是否低于0V
                            if (current_ad9833_voltage < 0.0) {
                                current_ad9833_voltage = 0.0;
                            }
                            // 转换为DAC值
                            base2_ad9833_amplitude = AD9833_VOLTAGE_TO_DAC(current_ad9833_voltage);
                            AD9833_AmpSet(base2_ad9833_amplitude);
                            
                            // 减小AD9954幅度
                            // 将当前DAC值转换回电压值
                            float current_ad9954_voltage = (float)base2_ad9954_amplitude * AD9954_VMAX_V / 16383.0;
                            // 减小0.1V
                            current_ad9954_voltage -= 0.1;
                            // 检查是否低于0V
                            if (current_ad9954_voltage < 0.0) {
                                current_ad9954_voltage = 0.0;
                            }
                            // 转换为DAC值
                            base2_ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(current_ad9954_voltage);
                            AD9954_Set_Amp(base2_ad9954_amplitude);
                            
                            printf("New amplitudes - AD9833: %d, AD9954: %d\n", base2_ad9833_amplitude, base2_ad9954_amplitude);
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键8可以有特殊功能
                            printf("Key 8 pressed in base3_function mode: Special function for base3\n");
                            // 在这里添加base3_function模式下按键8的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键8可以有特殊功能
                            printf("Key 8 pressed in base4_function mode: Special function for base4\n");
                            // 在这里添加base4_function模式下按键8的特殊功能
                            break;
                        default:
                            printf("Key 8 pressed\n");
                            break;
                    }
                    break;
                case 9:
                    // 按键9在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_BASE2:
                            // 在base2_function模式下，按键9可以有特殊功能
                            printf("Key 9 pressed in base2_function mode: Special function for base2\n");
                            // 在这里添加base2_function模式下按键9的特殊功能
                            break;
                        case FUNCTION_MODE_BASE3:
                            // 在base3_function模式下，按键9可以有特殊功能
                            printf("Key 9 pressed in base3_function mode: Special function for base3\n");
                            // 在这里添加base3_function模式下按键9的特殊功能
                            break;
                        case FUNCTION_MODE_BASE4:
                            // 在base4_function模式下，按键9可以有特殊功能
                            printf("Key 9 pressed in base4_function mode: Special function for base4\n");
                            // 在这里添加base4_function模式下按键9的特殊功能
                            break;
                        default:
                            printf("Key 9 pressed\n");
                            break;
                    }
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
        
        
        osDelay(20); 
    }
}


void base2_function(void){
    printf("Executing base2_function logic\n");
    // 设置当前模式为base2_function模式
    current_function_mode = FUNCTION_MODE_BASE2;
    
    // 初始化频率跟踪变量
    // base2_current_frequency = 1000.0; // 从1kHz开始
    
    // // 初始化幅度跟踪变量
    // // 检查电压是否超过最大值
    // float ad9833_init_voltage = 3.0;
    // if (ad9833_init_voltage > AD9833_VMAX_V) {
    //     ad9833_init_voltage = AD9833_VMAX_V;
    // }
    // base2_ad9833_amplitude = AD9833_VOLTAGE_TO_DAC(ad9833_init_voltage);
    
    // float ad9954_init_voltage = 3.0;
    // if (ad9954_init_voltage > AD9954_VMAX_V) {
    //     ad9954_init_voltage = AD9954_VMAX_V;
    // }
    // base2_ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(ad9954_init_voltage);
    
    // // 实现间隔5秒，让AD9954和AD9833依次从1kHz以100Hz递增到1MHz，并且输出的峰峰值为3V
    // double frequency = base2_current_frequency; // 从1kHz开始
    
    // 设置初始幅度
    AD9833_AmpSet(base2_ad9833_amplitude);
    AD9954_Set_Amp(base2_ad9954_amplitude);
    
    // 循环递增频率直到1MHz
    // while (frequency <= 1000000.0) {
    //     // 设置AD9833频率
    //     AD9833_WaveSeting(frequency, 0, SIN_WAVE, 0);
    //     printf("AD9833 Frequency set to: %.0f Hz\n", frequency);
        
    //     // 设置AD9954频率
    //     AD9954_Set_Fre(frequency);
    //     printf("AD9954 Frequency set to: %.0f Hz\n", frequency);

    //     osDelay(200);
        
    //     // 频率递增100Hz
    //     frequency += 100.0;
    //     // 更新跟踪变量
    //     base2_current_frequency = frequency;
    // }
    
    // printf("Frequency sweep completed\n");
}

void base3_function(void){
    printf("Executing base3_function logic\n");
    // 设置当前模式为base3_function模式
    current_function_mode = FUNCTION_MODE_BASE3;

    float ad9833_init_voltage = 0.7919;// 0.82
    if (ad9833_init_voltage > AD9833_VMAX_V) {
        ad9833_init_voltage = AD9833_VMAX_V;
    }
    base3_ad9833_amplitude = AD9833_VOLTAGE_TO_DAC(ad9833_init_voltage);
    
    float ad9954_init_voltage = 0.85;//微调，实际示波器 0.7919
    if (ad9954_init_voltage > AD9954_VMAX_V) {
        ad9954_init_voltage = AD9954_VMAX_V;
    }
    base3_ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(ad9954_init_voltage);

    AD9833_AmpSet(base3_ad9833_amplitude);
    AD9954_Set_Amp(base3_ad9954_amplitude);

    AD9833_WaveSeting(1000, 0, SIN_WAVE, 0);
    AD9954_Set_Fre(1000);

    printf("base3 completed\n");

}

void base4_function(void){
    printf("Executing base4_function logic\n");
    // 设置当前模式为base4_function模式
    current_function_mode = FUNCTION_MODE_BASE4;





}
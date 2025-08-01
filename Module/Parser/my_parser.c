#include "my_parser.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"
#include "AD9833.h"
#include "AD9954.h"
#include "my_timer_config.h"
#include "my_button_config.h"
#include "my_dds.h"

uint16_t g_dac_square_64[64] = {0};


extern TIM_HandleTypeDef htim4;
extern DAC_HandleTypeDef hdac1;
extern float DAC_ACTUAL_V_ZERO;
extern float DAC_ACTUAL_V_FULL;
extern float DAC_ACTUAL_SPAN;
extern DDS_Generator_t g_dds_generator; // 引用全局DDS生成器实例

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
double base2_current_frequency = 1000.0;  // 当前频率，初始为1kHz

// base3_function模式下的幅度跟踪变量
uint8_t base3_ad9833_amplitude = 255;  // AD9833幅度初始值
uint16_t base3_ad9954_amplitude = 16383;  // AD9954幅度初始值

// base4_function模式下的幅度和频率跟踪变量
double base4_ad9833_frequency = 100.0;   // AD9833当前频率
double base4_ad9954_frequency = 100.0;   // AD9954当前频率
double base4_dacdds_frequency = 100.0;   // DAC DDS当前频率
uint8_t base4_ad9833_amplitude = 255; // AD9833幅度初始值
uint16_t base4_ad9954_amplitude = 16383; // AD9954幅度初始值
uint8_t base4_dacdds_amplitude = 3; // DAC DDS幅度初始值
double base4_desired_model_output_voltage = 1.0; // 期望输出电压
double base4_desired_model_output_frequency = 100.0; // 期望输出频率

// base4_function模式下表格输入
/**
 * 第一个维度行是 100Hz, 200 Hz, ..., 3000 Hz 一共 30 行
 * 第二个维度列是 1.0, 1.1, ..., 1.9 ,2.0 一共 11 列
 * 每个元素是一个 double 类型的值，表示 DDS 在这种情况下要输出的幅度
 */


DDS_Generator_t g_dds_generator; // 宣告一個DDS產生器實例

const uint16_t g_arbitrary_waveform[WAVE_TABLE_SIZE] = {
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

double base4_table[30]={ // 传输比
    /* idx 从 0 开始到 29 */
    /* 频率 100Hz 到 3000Hz */
    /* f = idx * 100 + 100 = (idx + 1) * 100 */
    4.940968123,
    4.67027027,
    4.440614536,
    3.923152709,
    3.630353266,
    3.285404767,
    2.928154956,
    2.674937965,
    2.453920494,
    2.188634781,
    1.995005993,
    1.759580803,
    1.628499716,
    1.535362622,
    1.462351476,
    1.3797608,
    1.286160971,
    1.182819594,
    1.101226061,
    0.991769129,
    0.948720583,
    0.913787811,
    0.869057377,
    0.841037204,
    0.788785998,
    0.7560272,
    0.723588994,
    0.702215787,
    0.664887612,
    0.657913931
};


#define DDS_UPDATE_FREQUENCY 995062


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

    printf("Initializing DDS generator...\r\n");
    DAC_ACTUAL_SPAN = DAC_ACTUAL_V_FULL - DAC_ACTUAL_V_ZERO; // 计算实际的电压跨度

    // 1. 初始化DDS產生器
    DDS_Init(&g_dds_generator, &hdac1, DAC_CHANNEL_1, &htim4, DDS_UPDATE_FREQUENCY);

    printf("Setting waveform...\r\n");
    // 2. 設定要使用的波形
    DDS_SetWaveform(&g_dds_generator, g_arbitrary_waveform, WAVE_TABLE_SIZE);

    printf("Setting frequency...\r\n");
    // 3. 設定初始輸出頻率，例如 1000.0 Hz
    DDS_SetFrequency(&g_dds_generator, 1000.0f);

    printf("Starting DDS...\r\n");
    // 4. 啟動DDS引擎（這會自動啟動定時器和DMA）
    DDS_Start(&g_dds_generator);
    printf("DDS started.\r\n");

    // 设置DAC通道2的固定电压输出
    // HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    // HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2

    /* 初始化内存空间并且启动定时器和双 ADC */
    // memset(g_adc_dma_buffer, 0, ADC_SAMPLE_SIZE * sizeof(uint16_t));
    // SCB_CleanDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));

    // /* 【ADC 数据流】校准ADC 勿动 */
    // HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    // HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);

    // /* 【ADC 数据流】初始化同步采样的 ADC 模式 */
    // HAL_ADC_Start(&hadc2);
    // HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE);

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
            
                            break;
                        case FUNCTION_MODE_BASE3:
                     
                            break;
                        case FUNCTION_MODE_BASE4:
               
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
             
                            break;
                        case FUNCTION_MODE_BASE4:
         
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

                            break;
                        case FUNCTION_MODE_BASE3:

                            break;
                        case FUNCTION_MODE_BASE4:

                            break;
                        default:
                            printf("Key 3 pressed\n");
                            break;
                    }
                    break;
                case 4:
                    // 按键4在不同函数模式下的逻辑
                    switch (current_function_mode) {
                        case FUNCTION_MODE_NONE:
    
                            break;
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
                        case FUNCTION_MODE_NONE:

    
                            break;
                        case FUNCTION_MODE_BASE2:

                            break;
                        case FUNCTION_MODE_BASE3:

                            break;
                        case FUNCTION_MODE_BASE4:
  
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
            
                            break;
                        case FUNCTION_MODE_BASE3:
                        
                            break;
                        case FUNCTION_MODE_BASE4:
                        
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
                           
                            break;
                        case FUNCTION_MODE_BASE4:
                            
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
                          
                            break;
                        case FUNCTION_MODE_BASE3:
                           
                            break;
                        case FUNCTION_MODE_BASE4:
                            
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

    AD9833_WaveSeting(base2_current_frequency, 0, SIN_WAVE, 0);
    AD9954_Set_Fre(base2_current_frequency);
    
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
    printf("base2 completed\n");
}

// 重要
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

// 重要
void base4_function(void){
    printf("Executing base4_function logic\n");
    // 设置当前模式为base4_function模式
    current_function_mode = FUNCTION_MODE_BASE4;

    // 1. 根据 base4_desired_model_output_frequency 计算 base4_table 的索引
    uint16_t idx = (uint16_t)(base4_desired_model_output_frequency / 100.0) - 1;
    
    // 确保索引在有效范围内
    if (idx > 29) {
        idx = 29;
    }
    
    // 2. 从 base4_table 中获取传输比
    double transform_ratio = base4_table[idx];
    
    // 3. 根据传输比和 base4_desired_model_output_voltage 计算各 DDS 应该输出的电压
    double dds_output_voltage = base4_desired_model_output_voltage / transform_ratio;
    
    // 4. 设置 AD9833 的幅度和频率
    // 确保电压不超过AD9833的最大输出电压
    double ad9833_voltage = dds_output_voltage;
    if (ad9833_voltage > AD9833_VMAX_V) {
        ad9833_voltage = AD9833_VMAX_V;
    }
    
    // 将电压转换为DAC寄存器值并设置幅度
    uint8_t ad9833_dac_value = AD9833_VOLTAGE_TO_DAC(ad9833_voltage);
    AD9833_AmpSet(ad9833_dac_value);
    
    // 设置频率
    base4_ad9833_frequency = base4_desired_model_output_frequency;
    AD9833_WaveSeting(base4_ad9833_frequency, 0, SIN_WAVE, 0);
    
    // 5. 设置 AD9954 的幅度和频率
    // 确保电压不超过AD9954的最大输出电压
    double ad9954_voltage = dds_output_voltage;
    if (ad9954_voltage > AD9954_VMAX_V) {
        ad9954_voltage = AD9954_VMAX_V;
    }
    
    // 将电压转换为DAC寄存器值并设置幅度
    uint16_t ad9954_dac_value = AD9954_VOLTAGE_TO_DAC(ad9954_voltage);
    AD9954_Set_Amp(ad9954_dac_value);
    
    // 设置频率
    base4_ad9954_frequency = base4_desired_model_output_frequency;
    AD9954_Set_Fre(base4_ad9954_frequency);
    
    // 6. 设置 DAC DDS 的幅度和频率
    // 计算幅度值 (0.0 到 1.0)
    float dac_dds_amplitude = (float)((dds_output_voltage - DAC_ACTUAL_V_ZERO) / DAC_ACTUAL_SPAN);
    
    // 确保幅度值在有效范围内
    if (dac_dds_amplitude < 0.0f) {
        dac_dds_amplitude = 0.0f;
    } else if (dac_dds_amplitude > 1.0f) {
        dac_dds_amplitude = 1.0f;
    }
    
    // 设置幅度和频率
    base4_dacdds_frequency = (uint16_t)(base4_desired_model_output_frequency); // 转换为Hz
    DDS_SetAmplitude(&g_dds_generator, dac_dds_amplitude);
    DDS_SetFrequency(&g_dds_generator, (float)base4_dacdds_frequency);

    printf("base4 completed\n");
}


void improve1_function(void){
    printf("Executing improve1_function logic\n");
    // 设置当前模式为improve1_function模式
    current_function_mode = FUNCTION_MODE_IMPROVE1;



    printf("improve1 completed\n");
}

void improve2_function(void){
    printf("Executing improve2_function logic\n");
    // 设置当前模式为improve2_function模式
    current_function_mode = FUNCTION_MODE_IMPROVE2;


    printf("improve2 completed\n");
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
  // 這是DMA傳輸完整個緩衝區後的回呼 (Pong區完成)
  DDS_Callback_FullTransfer(&g_dds_generator);
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
  // 這是DMA傳輸完前半個緩衝區後的回呼 (Ping區完成)
  DDS_Callback_HalfTransfer(&g_dds_generator);
}


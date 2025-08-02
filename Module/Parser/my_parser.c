#include "my_parser.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"
#include "AD9833.h"
#include "AD9954.h"
#include "my_timer_config.h"
#include "my_button_config.h"
#include "my_dac_config.h"
#include "my_dds.h"

#include "my_uart_task.h"
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
double base3_ad9954_frequency = 1000.0;  // AD9954当前频率，初始为1kHz

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

extern TIM_HandleTypeDef htim6; // 定时器句柄
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


double base4_table[30][11] = {  // DDS幅度校准表
    /* 第一个维度: 频率索引 0-29 (对应100Hz-3000Hz, 步长100Hz) */
    /* 第二个维度: 电压索引 0-10 (对应1.0V-2.0V, 步长0.1V) */
    { // 频率 100Hz (索引 0)
        0.950000,  // 1.0V
        1.100000,  // 1.1V
        1.170000,  // 1.2V
        1.300000,  // 1.3V
        1.400000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.710000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 100Hz
    { // 频率 200Hz (索引 1)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.140000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.500000,  // 1.5V
        1.600000,  // 1.6V
        1.700000,  // 1.7000000000000002V
        1.710000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 200Hz
    { // 频率 300Hz (索引 2)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.140000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.500000,  // 1.5V
        1.600000,  // 1.6V
        1.700000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 300Hz
    { // 频率 400Hz (索引 3)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 400Hz
    { // 频率 500Hz (索引 4)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 500Hz
    { // 频率 600Hz (索引 5)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 600Hz
    { // 频率 700Hz (索引 6)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 700Hz
    { // 频率 800Hz (索引 7)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.410000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 800Hz
    { // 频率 900Hz (索引 8)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.210000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 900Hz
    { // 频率 1000Hz (索引 9)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1000Hz
    { // 频率 1100Hz (索引 10)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1100Hz
    { // 频率 1200Hz (索引 11)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.320000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1200Hz
    { // 频率 1300Hz (索引 12)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1300Hz
    { // 频率 1400Hz (索引 13)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.410000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1400Hz
    { // 频率 1500Hz (索引 14)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.210000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1500Hz
    { // 频率 1600Hz (索引 15)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.410000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1600Hz
    { // 频率 1700Hz (索引 16)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1700Hz
    { // 频率 1800Hz (索引 17)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1800Hz
    { // 频率 1900Hz (索引 18)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.210000,  // 1.3V
        1.310000,  // 1.4V
        1.410000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 1900Hz
    { // 频率 2000Hz (索引 19)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2000Hz
    { // 频率 2100Hz (索引 20)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2100Hz
    { // 频率 2200Hz (索引 21)
        0.930000,  // 1.0V
        1.100000,  // 1.1V
        1.130000,  // 1.2V
        1.220000,  // 1.3V
        1.310000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2200Hz
     { // 频率 2300Hz (索引 0)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2300Hz
    { // 频率 2400Hz (索引 1)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.700000,  // 1.9V
        1.900000  // 2.0V
    },  // 2400Hz
    { // 频率 2500Hz (索引 2)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2500Hz
    { // 频率 2600Hz (索引 3)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.700000,  // 1.9V
        1.900000  // 2.0V
    },  // 2600Hz
    { // 频率 2700Hz (索引 4)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2700Hz
    { // 频率 2800Hz (索引 5)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    },  // 2800Hz
    { // 频率 2900Hz (索引 6)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.800000  // 2.0V
    },  // 2900Hz
    { // 频率 3000Hz (索引 7)
        0.900000,  // 1.0V
        1.000000,  // 1.1V
        1.100000,  // 1.2V
        1.200000,  // 1.3V
        1.300000,  // 1.4V
        1.400000,  // 1.5V
        1.500000,  // 1.6V
        1.600000,  // 1.7000000000000002V
        1.700000,  // 1.8V
        1.800000,  // 1.9V
        1.900000  // 2.0V
    }  // 3000Hz
};


#define DDS_UPDATE_FREQUENCY 995062


void myParserTask(void const * argument)
{

    AD9954_Init(); // Initialize AD9954
    
    // Set amplitude to maximum for AD9954 (max value is 16383)
    AD9954_Set_Amp(16383/5);
    AD9954_Set_Fre(1000.0); // Set initial frequency to 1000 Hz
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
                            printf("Key 2 pressed in base2_function mode: Increasing all DDS amplitude\n");;
                            
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
                                uart_base4_function_with_params(base4_desired_model_output_voltage, base4_desired_model_output_frequency);
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
                            printf("Key 4 pressed in FUNCTION_MODE_NONE: Entering improve1_function\n");
                            improve1_function();
    
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
                            printf("Key 5 pressed in FUNCTION_MODE_NONE: Entering improve2_function\n");
                            improve2_function();
    
                            break;
                        case FUNCTION_MODE_IMPROVE2:
                            HAL_TIM_Base_Stop_IT(&htim6);
                            printf("Stopping improve2_function\n");
                            current_function_mode = FUNCTION_MODE_NONE; // 返回无模式
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
    
    AD9954_Set_Amp(base2_ad9954_amplitude);

    AD9954_Set_Fre(base2_current_frequency);
  
    printf("base2 completed\n");
}

// 重要
void base3_function(void){
    printf("Executing base3_function logic\n");
    // 设置当前模式为base3_function模式
    current_function_mode = FUNCTION_MODE_BASE3;

    
    float ad9954_init_voltage = 0.78;//微调，实际示波器 0.7919
    if (ad9954_init_voltage > AD9954_VMAX_V) {
        ad9954_init_voltage = AD9954_VMAX_V;
    }
    base3_ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(ad9954_init_voltage);

    AD9954_Set_Amp(base3_ad9954_amplitude);

    AD9954_Set_Fre(1000.0); // 设置初始频率为1000Hz

    printf("base3 completed\n");

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

    HAL_TIM_Base_Start_IT(&htim6);
    printf("improve2 completed\n");
}

float calibrate_AD9954_voltage(float desired_output_v, float frequency_hz) {
    // 校准AD9954电压，确保不超过最大值
    float setting = (desired_output_v - (7.650e-5f * frequency_hz) - 0.0454f) / 1.0186f;
    return setting;
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


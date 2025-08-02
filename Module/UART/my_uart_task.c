#include "my_uart_task.h"
#include "my_parameter_config.h" // 包含参数配置头文件
#include "my_button_config.h"    // 包含矩阵键盘配置
#include "my_zlcr_config.h"      // 包含DDS控制函数
#include "AD9833.h"              // 包含AD9833控制函数
#include "AD9954.h"              // 包含AD9954控制函数
#include <stdlib.h>  // 包含atof函数声明

#include "my_parser.h"           // 包含新系统入口函数
#include "my_dds.h"
#include "my_dac_config.h"


extern DDS_Generator_t g_dds_generator; 
extern const uint16_t g_sine_wave_64[WAVE_TABLE_SIZE];
extern const uint16_t g_square_wave_64[WAVE_TABLE_SIZE];
extern const uint16_t g_triangle_wave_64[WAVE_TABLE_SIZE];
extern float DAC_ACTUAL_V_ZERO;
extern float DAC_ACTUAL_V_FULL;
extern float DAC_ACTUAL_SPAN;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6; // 串口屏

char rx_buffer[RX_BUFFER_SIZE];   // 接收数据
uint8_t aRxBuffer;                // 接收中断缓冲
uint8_t uart1_rx_cnt = 0;         // 接收缓冲计数
uint8_t g_uart1_ex_flag = 0;      // UART1接收标志

char rx_buffer_uart6[RX_BUFFER_SIZE]; // 串口屏接收数据
uint8_t aRxBuffer_uart6;              // 串口屏接收中断
uint8_t uart6_rx_cnt = 0;             // 串口屏接收缓冲计数
uint8_t g_uart6_ex_flag = 0;          // UART6接收标

// 函数模式跟踪变量
extern uint8_t current_function_mode;  // 当前函数模式

// 串口屏当前页面跟踪变量
static uint8_t current_lcd_page = 2;  // 默认在基础功能2页面

// 串口屏设置的参数跟踪变量
static double lcd_base2_frequency = 1000.0;    // base2 频率，默认1kHz
static double lcd_base2_voltage = 1.0;         // base2 电压，默认1V
static double lcd_base3_frequency = 1000.0;    // base3 频率，默认1kHz  
static double lcd_base3_voltage = 0.78;        // base3 电压，默认0.78V
static double lcd_base4_frequency = 100.0;     // base4 频率，默认100Hz
static double lcd_base4_voltage = 1.0;         // base4 电压，默认1V

// S6信号重建功能触发标志位
uint8_t g_signal_reconstruction_trigger = 0;  // S6命令触发信号重建标志位

// base2_function模式下的幅度跟踪变量
extern uint8_t base2_ad9833_amplitude;  // AD9833幅度初始值
extern uint16_t base2_ad9954_amplitude;  // AD9954幅度初始值
extern double base2_current_frequency;  // 当前频率，初始为1kHz

// base3_function模式下的幅度跟踪变量
extern uint8_t base3_ad9833_amplitude;  // AD9833幅度初始值
extern uint16_t base3_ad9954_amplitude;  // AD9954幅度初始值
extern double base3_ad9954_frequency;  // AD9954当前频率

// base4_function模式下的幅度跟踪变量
extern uint8_t base4_ad9833_amplitude;  // AD9833幅度初始值
extern uint16_t base4_ad9954_amplitude;  // AD9954幅度初始值
extern double base4_ad9833_frequency;    // AD9833当前频率
extern double base4_ad9954_frequency;    // AD9954当前频率

extern double base4_desired_model_output_frequency;
extern double base4_desired_model_output_voltage; // 期望输出电压

// 引用 base4_table 数组（来自 my_parser.c）
extern double base4_table[30];

void StartUARTProcessingTask(void const * argument)
{
    // UART任务代码
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&aRxBuffer, 1);
    HAL_UART_Receive_IT(&huart6, (uint8_t *)&aRxBuffer_uart6, 1);
    for(;;)
    {
        /* 串口接收中断 */
        if (g_uart1_ex_flag)
        {
            g_uart1_ex_flag = 0;

            // 解析接收到的字符串
            parse_uart_command(rx_buffer);

            uart1_rx_cnt = 0;
            memset(rx_buffer, 0, sizeof(rx_buffer));
        }

        if (g_uart6_ex_flag)
        {
            g_uart6_ex_flag = 0;

            // 解析串口屏接收到的字符串
            printf("Received from UART6: %s\n", rx_buffer_uart6);
            parse_serial_lcd_command(rx_buffer_uart6);

            uart6_rx_cnt = 0;
            memset(rx_buffer_uart6, 0, sizeof(rx_buffer_uart6));
        }

        osDelay(20); // 延时20ms
    }
}

/**
 * @brief 解析UART命令
 * @param cmd 接收到的命令字符串
 */
void parse_uart_command(char* cmd)
{
    // 解析命令格式: CMD:PARAM[:VALUE]
    // 支持两部分命令(如GET:ALL)和三部分命令(如SET:DDS_FREQ:1000)
    char *token;
    char *cmd_type;
    char *param;
    char *value = NULL; // 默认值为NULL

    // 使用strtok分割字符串
    token = strtok(cmd, ":");
    if (token == NULL) {
        printf("Invalid command format\n");
        return;
    }
    cmd_type = token;

    token = strtok(NULL, ":");
    if (token == NULL) {
        printf("Invalid command format\n");
        return;
    }
    param = token;

    // 尝试获取第三个部分(可选)
    token = strtok(NULL, ":");
    if (token != NULL) {
        value = token;
    }

    // 处理SET命令
    if (strcmp(cmd_type, "SET") == 0) {
        // SET命令必须有值
        if (value == NULL) {
            printf("SET command requires a value\n");
            return;
        }
        handle_set_command(param, value);
    }
    // 处理GET命令
    else if (strcmp(cmd_type, "GET") == 0) {
        handle_get_command(param);
        printf("GET command processed: %s\n", param);
    }
    // 兼容旧格式
    else {
        handle_legacy_command(cmd);
    }
}

/**
 * @brief 处理SET命令
 * @param param 参数名
 * @param value 参数值
 */
void handle_set_command(char* param, char* value)
{
    if (strcmp(param, "DDS_FREQ") == 0) {
        g_desired_dds_frequency = (uint32_t)strtoul(value, NULL, 10);
        printf("Set DDS frequency: %lu Hz\n", g_desired_dds_frequency);
        // 更新DDS频率
        if (g_desired_dds_type == DDS_TYPE_AD9833) {
            AD9833_WaveSeting(g_desired_dds_frequency, g_desired_dds_phase, SIN_WAVE, 0);
        } else {
            AD9954_Set_Fre(g_desired_dds_frequency);
        }
    }
    else if (strcmp(param, "ADC_RATE") == 0) {
        g_desired_ADC_sample_rate_Hz = (uint32_t)strtoul(value, NULL, 10);
        printf("Set ADC sample rate: %lu Hz\n", g_desired_ADC_sample_rate_Hz);
    }
    else if (strcmp(param, "DDS_TYPE") == 0) {
        if (strcmp(value, "AD9833") == 0) {
            g_desired_dds_type = DDS_TYPE_AD9833;
            printf("Set DDS type: AD9833\n");
        } else if (strcmp(value, "AD9954") == 0) {
            g_desired_dds_type = DDS_TYPE_AD9954;
            printf("Set DDS type: AD9954\n");
        } else {
            printf("Invalid DDS type\n");
        }
    }
    else if (strcmp(param, "DDS_PHASE") == 0) {
        g_desired_dds_phase = (uint32_t)strtoul(value, NULL, 10);
        printf("Set DDS phase: %lu degrees\n", g_desired_dds_phase);
        // 更新DDS相位
        if (g_desired_dds_type == DDS_TYPE_AD9954) {
            AD9954_Set_Phase(g_desired_dds_phase);
        }
    }
    else if (strcmp(param, "DDS_AMP") == 0) {
        float voltage = atof(value);
        if (g_desired_dds_type == DDS_TYPE_AD9833) {
            g_desired_dds_amplitude = AD9833_VOLTAGE_TO_DAC(voltage);
        } else {
            g_desired_dds_amplitude = AD9954_VOLTAGE_TO_DAC((voltage));
            printf("Calibrated AD9954 voltage: %.2f V -> %d in %lu Hz\n", voltage, g_desired_dds_amplitude, g_desired_dds_frequency);
        }
        printf("Set DDS amplitude: %.2f V (%lu)\n", voltage, g_desired_dds_amplitude);
        // 更新DDS幅度
        if (g_desired_dds_type == DDS_TYPE_AD9833) {
            AD9833_AmpSet(g_desired_dds_amplitude);
        } else {
            AD9954_Set_Amp(g_desired_dds_amplitude);
        }
    }
    else if (strcmp(param, "DAC_WAVE") == 0) {
        if (strcmp(value, "SINE") == 0) {
            g_desired_DAC_output_waveform = 0; // 正弦波
        } else if (strcmp(value, "SQUARE") == 0) {
            g_desired_DAC_output_waveform = 1; // 方波
        } else if (strcmp(value, "TRIANGLE") == 0) {
            g_desired_DAC_output_waveform = 2; // 三角波
        }
        printf("Set DAC waveform: %s\n", value);
        // 更新DAC波形
        update_dac_waveform();
    }
    else if (strcmp(param, "DAC_FREQ") == 0) {
        g_desired_DAC_output_frequency = (uint32_t)strtoul(value, NULL, 10);
        printf("Set DAC frequency: %lu Hz\n", g_desired_DAC_output_frequency);
        // 更新DAC频率
        update_dac_frequency();
    }
    else if (strcmp(param, "DAC_AMP") == 0) {
        g_desired_DAC_single_output_amplitude = atof(value);
        printf("Set DAC amplitude: %.2f V\n", g_desired_DAC_single_output_amplitude);
        // 更新DAC幅度
        update_dac_amplitude();
    }
    else if (strcmp(param, "RELAY") == 0) {
        g_desired_switch2which_relay = (uint8_t)strtoul(value, NULL, 10);
        printf("Set relay: %d\n", g_desired_switch2which_relay);
        // 更新继电器状态
        update_relay_state();
    }
    else if (strcmp(param, "FUNC") == 0) {
        if (strcmp(value, "LCR") == 0) {
            g_desired_function_state = LCR_STATE;
        } else if (strcmp(value, "SPECTRUM") == 0) {
            g_desired_function_state = SPECTRUM_STATE;
        } else if (strcmp(value, "TIME") == 0) {
            g_desired_function_state = TIME_STATE;
        } else if (strcmp(value, "DIY") == 0) {
            g_desired_function_state = DIY_STATE;
        }
        printf("Set function state: %s\n", value);
    }
    else if (strcmp(param, "BASE4") == 0) {
        // 处理 SET:BASE4:voltage&frequency 指令
        // value 格式应该是 "voltage&frequency"，例如 "1.5&1000"
        char* voltage_str = strtok(value, "&");
        char* frequency_str = strtok(NULL, "&");
        
        if (voltage_str != NULL && frequency_str != NULL) {
            double voltage = atof(voltage_str);
            double frequency = atof(frequency_str);
            
            printf("Set BASE4 parameters - Voltage: %.2f V, Frequency: %.2f Hz\n", voltage, frequency);
            
            // 调用带参数的 base4 函数
            uart_base4_function_with_params(voltage, frequency);
        } else {
            printf("Invalid BASE4 parameter format. Expected: voltage&frequency\n");
        }
    }
    else {
        printf("Unknown parameter: %s\n", param);
    }
}

/**
 * @brief 处理串口屏SET命令
 * @param param 参数名
 * @param value 参数值
 */
void handle_serial_lcd_set_command(char* param, char* value)
{
    // 解析参数值
    float voltage = atof(value);
    
    // 根据当前页面和参数类型更新对应的参数跟踪变量
    if (strcmp(param, "DDS_AMP") == 0) {
        // 更新电压跟踪变量
        if (current_lcd_page == 2) {
            // 在基础功能2页面
            lcd_base2_voltage = voltage;
            printf("Base2 voltage updated: %.2f V\n", voltage);
        } else if (current_lcd_page == 3) {
            // 在基础功能3页面
            lcd_base3_voltage = voltage;
            printf("Base3 voltage updated: %.2f V\n", voltage);
        } else if (current_lcd_page == 4) {
            // 在基础功能4页面
            lcd_base4_voltage = voltage;
            printf("Base4 voltage updated: %.2f V\n", voltage);
        }
    } else if (strcmp(param, "DDS_FREQ") == 0) {
        // 更新频率跟踪变量
        double frequency = atof(value);
        if (current_lcd_page == 2) {
            lcd_base2_frequency = frequency;
            printf("Base2 frequency updated: %.2f Hz\n", frequency);
        } else if (current_lcd_page == 3) {
            lcd_base3_frequency = frequency;
            printf("Base3 frequency updated: %.2f Hz\n", frequency);
        } else if (current_lcd_page == 4) {
            lcd_base4_frequency = frequency;
            printf("Base4 frequency updated: %.2f Hz\n", frequency);
        }
    } else {
        printf("Unknown parameter: %s\n", param);
    }
}

/**
 * @brief 处理GET命令
 * @param param 参数名
 */
void handle_get_command(char* param)
{
    if (strcmp(param, "ALL") == 0) {
        printf("=== System Parameters ===\n");
        printf("DDS Frequency: %lu Hz\n", g_desired_dds_frequency);
        printf("DDS Type: %s\n", g_desired_dds_type == DDS_TYPE_AD9833 ? "AD9833" : "AD9954");
        printf("DDS Phase: %lu degrees\n", g_desired_dds_phase);
        printf("DDS Amplitude: %lu\n", g_desired_dds_amplitude);
        printf("ADC Sample Rate: %lu Hz\n", g_desired_ADC_sample_rate_Hz);
        printf("DAC Waveform: %d\n", g_desired_DAC_output_waveform);
        printf("DAC Frequency: %lu Hz\n", g_desired_DAC_output_frequency);
        printf("DAC Amplitude: %.2f V\n", g_desired_DAC_single_output_amplitude);
        printf("Relay: %d\n", g_desired_switch2which_relay);
        switch (g_desired_function_state) {
            case LCR_STATE: printf("Function State: LCR\n"); break;
            case SPECTRUM_STATE: printf("Function State: SPECTRUM\n"); break;
            case TIME_STATE: printf("Function State: TIME\n"); break;
            case DIY_STATE: printf("Function State: DIY\n"); break;
        }
    }
    else if (strcmp(param, "DDS_FREQ") == 0) {
        printf("DDS Frequency: %lu Hz\n", g_desired_dds_frequency);
    }
    else if (strcmp(param, "ADC_RATE") == 0) {
        printf("ADC Sample Rate: %lu Hz\n", g_desired_ADC_sample_rate_Hz);
    }
    // 其他参数的查询实现...
    else {
        printf("Unknown parameter: %s\n", param);
    }
}

/**
 * @brief 处理兼容旧格式的命令
 * @param cmd 命令字符串
 */
void handle_legacy_command(char* cmd)
{
    // 解析接收到的字符串，支持设置ADC采样率或DDS频率
    if (cmd[0] == 'A' || cmd[0] == 'a') {
        // 设置ADC采样率，格式如"A100000"表示设置ADC采样率为100000Hz
        g_desired_ADC_sample_rate_Hz = (uint32_t)strtoul(&cmd[1], NULL, 10);
        printf("Set ADC sample rate: %lu Hz\n", g_desired_ADC_sample_rate_Hz);
    } else if (cmd[0] == 'D' || cmd[0] == 'd') {
        // 设置DDS频率，格式如"D100000"表示设置DDS频率为100000Hz
        g_desired_dds_frequency = (uint32_t)strtoul(&cmd[1], NULL, 10);
        printf("Set DDS frequency: %lu Hz\n", g_desired_dds_frequency);
    } else {
        // 兼容旧格式，直接设置ADC采样率
        g_desired_ADC_sample_rate_Hz = (uint32_t)strtoul(cmd, NULL, 10);
        printf("Set ADC sample rate (legacy format): %lu Hz\n", g_desired_ADC_sample_rate_Hz);
    }
}

/**
 * @brief 更新DAC波形
 */
void update_dac_waveform(void)
{
    // 波形更新逻辑将在DAC任务中实现
    printf("DAC waveform update requested\n");
    DDS_Stop(&g_dds_generator);
    if (g_desired_DAC_output_waveform == 0) {
        // 正弦波
        DDS_SetWaveform(&g_dds_generator, g_sine_wave_64, WAVE_TABLE_SIZE);
    } else if (g_desired_DAC_output_waveform == 1) {
        // 方波
        DDS_SetWaveform(&g_dds_generator, g_square_wave_64, WAVE_TABLE_SIZE);
    } else if (g_desired_DAC_output_waveform == 2) {
        // 三角波
        DDS_SetWaveform(&g_dds_generator, g_triangle_wave_64, WAVE_TABLE_SIZE);
    }

    DDS_Start(&g_dds_generator);

}

/**
 * @brief 更新DAC频率
 */
void update_dac_frequency(void)
{
    // 频率更新逻辑将在DAC任务中实现
    printf("DAC frequency update requested\n");
    DDS_SetFrequency(&g_dds_generator, g_desired_DAC_output_frequency);

}

/**
 * @brief 更新DAC幅度
 */
void update_dac_amplitude(void)
{
    // 幅度更新逻辑将在DAC任务中实现
    printf("DAC amplitude update requested\n");
    /**
     * g_desired_DAC_single_output_amplitude 是 0~3.3v
     * DDS 参数的 amplitude 是 0.0f 到 1.0f
     */

    float calculated_amplitude = (g_desired_DAC_single_output_amplitude - DAC_ACTUAL_V_ZERO) / DAC_ACTUAL_SPAN;
    DDS_SetAmplitude(&g_dds_generator, calculated_amplitude);
}

/**
 * @brief 更新继电器状态
 */
void update_relay_state(void)
{
    // 根据g_desired_switch2which_relay设置对应的GPIO引脚
    // 先重置所有引脚
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
    
    switch(g_desired_switch2which_relay) {
        case 0: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET); break;
        case 1: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET); break;
        case 2: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET); break;
        case 3: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); break;
        default: break;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

    if (huart->Instance == USART1) {
        // 处理接收到的数据
        /* 注意： 需要回调时，不应修改该函数、可在用户文件中实现 HAL_UART_TxCpltCallback */
        if(uart1_rx_cnt >= RX_BUFFER_SIZE-1)  //溢出判断
        {
            uart1_rx_cnt = 0;
            memset(rx_buffer,0x00,sizeof(rx_buffer));
        }
        else
        {
            if(aRxBuffer == 0x0D) // 只用回车作为结束符
            {
                rx_buffer[uart1_rx_cnt] = 0; // 字符串结尾
                g_uart1_ex_flag = 1;
            }
            else
            {
                rx_buffer[uart1_rx_cnt++] = aRxBuffer;
            }
        }

        HAL_UART_Receive_IT(&huart1, (uint8_t *)&aRxBuffer, 1);   //再开启接收中断
    }

    if (huart->Instance == USART6) {
        // 处理串口屏接收到的数据
        if(uart6_rx_cnt >= RX_BUFFER_SIZE-1)  //溢出判断
        {
            uart6_rx_cnt = 0;
            memset(rx_buffer_uart6,0x00,sizeof(rx_buffer_uart6));
        }
        else
        {
            if(aRxBuffer_uart6 == 0x0D) // 只用回车作为结束符
            {
                rx_buffer_uart6[uart6_rx_cnt] = 0; // 字符串结尾
                g_uart6_ex_flag = 1;
            }
            else
            {
                rx_buffer_uart6[uart6_rx_cnt++] = aRxBuffer_uart6;
            }
        }

        HAL_UART_Receive_IT(&huart6, (uint8_t *)&aRxBuffer_uart6, 1);   //再开启接收中断
    }
}

/**
 * @brief 解析串口屏命令
 * @param cmd 接收到的命令字符串
 */
void parse_serial_lcd_command(char* cmd)
{
    // 串口屏命令解析
    // S2 - 开始测试，调用base2_function函数
    // S3 - 开始测试，调用base3_function函数
    // S4 - 开始测试，调用base4_function函数
    // I2 - 进入基础功能2页面
    // I3 - 进入基础功能3页面
    // I4 - 进入基础功能4页面

    if (strcmp(cmd, "S2") == 0) {
        // 串口屏发送"S2"命令，表示开始测试，MCU接收到该命令后会调用带参数的base2函数
        printf("Received S2 command, calling uart_base2_function_with_params\n");
        current_lcd_page = 2;  // 更新当前页面跟踪变量
        uart_base2_function_with_params(lcd_base2_frequency, lcd_base2_voltage);
    }
    else if (strcmp(cmd, "S3") == 0) {
        // 串口屏发送"S3"命令，表示开始测试，MCU接收到该命令后会调用带参数的base3函数
        printf("Received S3 command, calling uart_base3_function_with_params\n");
        current_lcd_page = 3;  // 更新当前页面跟踪变量
        uart_base3_function_with_params(lcd_base3_frequency, lcd_base3_voltage);
    }
    else if (strcmp(cmd, "S4") == 0) {
        // 串口屏发送"S4"命令，表示开始测试，MCU接收到该命令后会调用带参数的base4函数
        printf("Received S4 command, calling uart_base4_function_with_params\n");
        current_lcd_page = 4;  // 更新当前页面跟踪变量
        uart_base4_function_with_params(lcd_base4_voltage, lcd_base4_frequency);
    }
    else if (strcmp(cmd, "S6") == 0) {
        // 串口屏发送"S6"命令，表示触发improve2波形重建功能
        printf("Received S6 command, triggering signal reconstruction\n");
        g_signal_reconstruction_trigger = 1;  // 设置信号重建触发标志位
        current_function_mode = FUNCTION_MODE_IMPROVE2;  // 设置为improve2模式
    }
    else if (strcmp(cmd, "I2") == 0) {
        // 串口屏发送"I2"命令，表示进入基础功能2页面
        printf("Entered base2 page\n");
        current_lcd_page = 2;  // 更新当前页面跟踪变量
    }
    else if (strcmp(cmd, "I3") == 0) {
        // 串口屏发送"I3"命令，表示进入基础功能3页面
        printf("Entered base3 page\n");
        current_lcd_page = 3;  // 更新当前页面跟踪变量
    }
    else if (strcmp(cmd, "I4") == 0) {
        // 串口屏发送"I4"命令，表示进入基础功能4页面
        printf("Entered base4 page\n");
        current_lcd_page = 4;  // 更新当前页面跟踪变量
    }
    else {
        // 检查是否是SET命令
        char *token;
        char *cmd_type;
        char *param;
        char *value = NULL;
        
        // 复制命令字符串，因为strtok会修改原字符串
        char cmd_copy[256];
        strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
        cmd_copy[sizeof(cmd_copy) - 1] = '\0';
        
        // 使用strtok分割字符串
        token = strtok(cmd_copy, ":");
        if (token != NULL) {
            cmd_type = token;
            
            token = strtok(NULL, ":");
            if (token != NULL) {
                param = token;
                
                // 尝试获取第三个部分(可选)
                token = strtok(NULL, ":");
                if (token != NULL) {
                    value = token;
                }
                
                // 处理SET命令
                if (strcmp(cmd_type, "SET") == 0) {
                    // SET命令必须有值
                    if (value != NULL) {
                        handle_serial_lcd_set_command(param, value);
                    } else {
                        printf("Serial LCD SET command requires a value\n");
                    }
                } else {
                    printf("Unknown serial LCD command: %s\n", cmd);
                }
            } else {
                printf("Unknown serial LCD command: %s\n", cmd);
            }
        } else {
            printf("Unknown serial LCD command: %s\n", cmd);
        }
    }
}

/**
 * @brief 带参数的 base4 函数，逻辑与 my_parser.c 中的 base4_function() 相同
 * @param voltage 期望输出电压
 * @param frequency 期望输出频率
 */
void uart_base4_function_with_params(double voltage, double frequency)
{
    printf("Executing uart_base4_function_with_params - Voltage: %.2f V, Frequency: %.2f Hz\n", voltage, frequency);
    
    // 1. 根据频率计算 base4_table 的索引
    uint16_t idx = (uint16_t)(frequency / 100.0) - 1;
    
    // 确保索引在有效范围内
    if (idx > 29) {
        idx = 29;
    }
    
    // 2. 从 base4_table 中获取传输比
    double transform_ratio = base4_table[idx];
    
    // 3. 根据传输比和期望输出电压计算各 DDS 应该输出的电压
    double dds_output_voltage = voltage / transform_ratio;
    
    // 4. 设置 AD9954 的幅度和频率
    double final_ad9954_voltage = dds_output_voltage;
    printf("应该在表中查找到的传输比： %.6f, 对应的频率 %.6f Hz\n", transform_ratio, frequency);
    printf("AD9954 最后要设的电压是： %.6f V\n", final_ad9954_voltage);
    
    // 5. 将电压转换为DAC寄存器值并设置幅度
    AD9954_Set_Amp(AD9954_VOLTAGE_TO_DAC(final_ad9954_voltage)); // 设置幅度
    
    // 6. 设置频率
    AD9954_Set_Fre(frequency);
    
    printf("uart_base4_function_with_params completed\n");
}

/**
 * @brief 带参数的 base2 函数，逻辑与 my_parser.c 中的 base2_function() 相同
 * @param frequency 频率 (Hz)
 * @param ad9954_voltage AD9954 输出电压 (V)
 */
void uart_base2_function_with_params(double frequency, double ad9954_voltage)
{
    printf("Executing uart_base2_function_with_params - Frequency: %.2f Hz, AD9954 Voltage: %.2f V\n", frequency, ad9954_voltage);
    
    // 设置当前模式为base2_function模式
    current_function_mode = FUNCTION_MODE_BASE2;
    
    // 设置 AD9954 幅度
    uint16_t ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(ad9954_voltage);
    AD9954_Set_Amp(ad9954_amplitude);
    
    // 设置 AD9954 频率
    AD9954_Set_Fre(frequency);
    
    printf("uart_base2_function_with_params completed\n");
}

/**
 * @brief 带参数的 base3 函数，逻辑与 my_parser.c 中的 base3_function() 相同
 * @param frequency 频率 (Hz) 
 * @param ad9954_voltage AD9954 输出电压 (V)
 */
void uart_base3_function_with_params(double frequency, double ad9954_voltage)
{
    printf("Executing uart_base3_function_with_params - Frequency: %.2f Hz, AD9954 Voltage: %.2f V\n", frequency, ad9954_voltage);
    
    // 设置当前模式为base3_function模式
    current_function_mode = FUNCTION_MODE_BASE3;
    
    // 设置 AD9954 幅度
    uint16_t ad9954_amplitude = AD9954_VOLTAGE_TO_DAC(ad9954_voltage);
    AD9954_Set_Amp(ad9954_amplitude);
    
    // 设置 AD9954 频率
    AD9954_Set_Fre(frequency);
    
    printf("uart_base3_function_with_params completed\n");
}

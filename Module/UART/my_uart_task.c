#include "my_uart_task.h"
#include "my_parameter_config.h" // 包含参数配置头文件
#include "my_button_config.h"    // 包含矩阵键盘配置
#include "my_zlcr_config.h"      // 包含DDS控制函数
#include "AD9833.h"              // 包含AD9833控制函数
#include "AD9954.h"              // 包含AD9954控制函数

extern UART_HandleTypeDef huart1;

char rx_buffer[RX_BUFFER_SIZE];   // 接收数据
uint8_t aRxBuffer;                // 接收中断缓冲
uint8_t uart1_rx_cnt = 0;         // 接收缓冲计数
uint8_t g_uart1_ex_flag = 0;      // UART1接收标志

// DDS类型定义
#define DDS_TYPE_AD9833 1
#define DDS_TYPE_AD9954 0

void StartUARTProcessingTask(void const * argument)
{
    // UART任务代码
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&aRxBuffer, 1);
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

        osDelay(20); // 延时20ms
    }
}

/**
 * @brief 解析UART命令
 * @param cmd 接收到的命令字符串
 */
void parse_uart_command(char* cmd)
{
    // 解析命令格式: CMD:PARAM:VALUE
    char *token;
    char *cmd_type;
    char *param;
    char *value;

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

    token = strtok(NULL, ":");
    if (token == NULL) {
        printf("Invalid command format\n");
        return;
    }
    value = token;

    // 处理SET命令
    if (strcmp(cmd_type, "SET") == 0) {
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
        g_desired_dds_amplitude = (uint32_t)strtoul(value, NULL, 10);
        printf("Set DDS amplitude: %lu\n", g_desired_dds_amplitude);
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
    else {
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
}

/**
 * @brief 更新DAC频率
 */
void update_dac_frequency(void)
{
    // 频率更新逻辑将在DAC任务中实现
    printf("DAC frequency update requested\n");
}

/**
 * @brief 更新DAC幅度
 */
void update_dac_amplitude(void)
{
    // 幅度更新逻辑将在DAC任务中实现
    printf("DAC amplitude update requested\n");
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

  /* 注意： 需要回调时，不应修改该函数、
           可在用户文件中实现 HAL_UART_TxCpltCallback */
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

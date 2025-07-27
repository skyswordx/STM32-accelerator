#include "my_uart_task.h"
#include "my_button_config.h" // 包含矩阵键盘配置

uint32_t g_desired_ADC_sample_rate_Hz = 2000000; // 默认采样率为2MHz
extern UART_HandleTypeDef huart1;

char rx_buffer[RX_BUFFER_SIZE];   //接收数据
uint8_t aRxBuffer;						 //接收中断缓冲
uint8_t uart1_rx_cnt = 0;			 //接收缓冲计数
uint8_t g_uart1_ex_flag = 0; // UART1接收标志


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
            
            // 解析接收到的字符串，支持设置ADC采样率或DDS频率
            if (rx_buffer[0] == 'A' || rx_buffer[0] == 'a') {
                // 设置ADC采样率，格式如"A100000"表示设置ADC采样率为100000Hz
                g_desired_ADC_sample_rate_Hz = (uint32_t)strtoul(&rx_buffer[1], NULL, 10);
                printf("Set ADC sample rate: %lu Hz\n", g_desired_ADC_sample_rate_Hz);
            } else if (rx_buffer[0] == 'D' || rx_buffer[0] == 'd') {
                // 设置DDS频率，格式如"D100000"表示设置DDS频率为100000Hz
                g_desired_dds_frequency = (uint32_t)strtoul(&rx_buffer[1], NULL, 10);
                printf("Set DDS frequency: %lu Hz\n", g_desired_dds_frequency);
            } else {
                // 兼容旧格式，直接设置ADC采样率
                g_desired_ADC_sample_rate_Hz = (uint32_t)strtoul(rx_buffer, NULL, 10);
                printf("Set ADC sample rate (legacy format): %lu Hz\n", g_desired_ADC_sample_rate_Hz);
            }
            
            uart1_rx_cnt = 0;
            memset(rx_buffer, 0, sizeof(rx_buffer));
        }

        osDelay(20); // 延时20ms
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

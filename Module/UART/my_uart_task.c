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
            // 将接收到的数字字符串转为uint32_t
            g_desired_ADC_sample_rate_Hz = (uint32_t)strtoul(rx_buffer, NULL, 10);
            printf("set sample rate: %lu\n", g_desired_ADC_sample_rate_Hz);
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




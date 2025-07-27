#ifndef MY_UART_TASK_H
#define MY_UART_TASK_H

#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"

#define RX_BUFFER_SIZE  256     //最大接收字节数

extern char rx_buffer[RX_BUFFER_SIZE];   //接收数据
extern uint8_t aRxBuffer;						 //接收中断缓冲
extern uint8_t uart1_rx_cnt;			 //接收缓冲计数
extern uint8_t g_uart1_ex_flag; // UART1接收标志

// DDS频率相关变量声明
extern uint32_t g_desired_dds_frequency; // 用户期望设置的DDS频率

void StartUARTProcessingTask(void const * argument);

#endif /* MY_UART_TASK_H */
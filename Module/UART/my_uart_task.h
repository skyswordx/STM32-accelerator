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

// S6信号重建功能触发标志位声明
extern uint8_t g_signal_reconstruction_trigger;  // S6命令触发信号重建标志位

// 函数声明
void StartUARTProcessingTask(void const * argument);
void parse_uart_command(char* cmd);
void handle_set_command(char* param, char* value);
void handle_get_command(char* param);
void handle_legacy_command(char* cmd);
void update_dac_waveform(void);
void update_dac_frequency(void);
void update_dac_amplitude(void);
void update_relay_state(void);

// 新添加的 base4 函数，带参数版本
void uart_base4_function_with_params(double voltage, double frequency);

// 新添加的带参数版本的 base2 和 base3 函数
void uart_base2_function_with_params(double frequency, double ad9954_voltage);
void uart_base3_function_with_params(double frequency, double ad9954_voltage);

void parse_serial_lcd_command(char* cmd);



#endif /* MY_UART_TASK_H */
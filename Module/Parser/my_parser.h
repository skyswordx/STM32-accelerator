#ifndef MY_PARSER_H
#define MY_PARSER_H

// 函数模式宏定义
#define FUNCTION_MODE_NONE    0  // 无模式
#define FUNCTION_MODE_BASE2   2  // base2_function模式
#define FUNCTION_MODE_BASE3   3  // base3_function模式
#define FUNCTION_MODE_BASE4   4  // base4_function模式

#define FUNCTION_MODE_IMPROVE1 5 // improve1_function模式
#define FUNCTION_MODE_IMPROVE2 6 // improve2_function模式

// 电压转换宏定义
#define AD9833_VMAX_V 3.6
#define AD9954_VMAX_V 4.16
#define AD9833_VOLTAGE_TO_DAC(voltage) ((uint8_t)((voltage) * 255.0 / AD9833_VMAX_V))
// AD9954幅度设置范围是0-16383，4.16V峰峰值对应大约9830
#define AD9954_VOLTAGE_TO_DAC(voltage) ((uint16_t)((voltage) * 16383.0 / AD9954_VMAX_V))

void myParserTask(void const * argument);
void base2_function(void);
void base3_function(void);
void base4_function(void);

void improve1_function(void);
void improve2_function(void);


#endif // MY_PARSER_H
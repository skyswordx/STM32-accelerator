#ifndef MY_PARSER_H
#define MY_PARSER_H

// 电压到DAC参数的转换宏定义
#define AD9833_VOLTAGE_TO_DAC(voltage) ((uint8_t)((voltage) * 255.0 / 5.0))   // AD9833幅度范围0-255
#define AD9954_VOLTAGE_TO_DAC(voltage) ((uint16_t)((voltage) * 16383.0 / 5.0)) // AD9954幅度范围0-16383

void myParserTask(void const * argument);
void base2_function(void);
void base3_function(void);
void base4_function(void);

#endif // MY_PARSER_H
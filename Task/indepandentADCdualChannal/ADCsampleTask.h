#ifndef __ADC_SAMPLE_TASK_H__
#define __ADC_SAMPLE_TASK_H__


#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* ADC采样任务配置 */
#define ADC_CHANNEL_COUNT           6       // 采样通道数量
#define ADC_SAMPLE_WINDOW_SIZE      10      // 滑动窗口大小
#define ADC_SAMPLE_PERIOD_MS        10      // 采样周期 10ms
#define ADC_TIMEOUT_MS              100     // ADC超时时间

/* ADC通道定义 */
typedef enum {
    ADC_CH_IN3 = 0,     // PA6  -> ADC1_INP3
    ADC_CH_IN4,         // PC4  -> ADC1_INP4  
    ADC_CH_IN5,         // PB1  -> ADC1_INP5
    ADC_CH_IN7,         // PA7  -> ADC1_INP7
    ADC_CH_IN8,         // PC5  -> ADC1_INP8
    ADC_CH_IN9,         // PB0  -> ADC1_INP9
} adc_channel_index_t;

/* ADC通道映射结构体 */
typedef struct {
    uint32_t channel;           // HAL库通道定义
    const char* name;           // 通道名称
    float voltage_scale;        // 电压换算系数
} adc_channel_config_t;

/* ADC采样数据结构体 */
typedef struct {
    uint16_t raw_values[ADC_SAMPLE_WINDOW_SIZE];    // 原始ADC值滑动窗口
    uint32_t raw_sum;                               // 原始值累加和
    uint16_t raw_average;                           // 原始值平均
    float voltage_average;                          // 电压平均值
    uint8_t window_index;                           // 窗口索引
    uint8_t window_full;                            // 窗口是否已满
    uint32_t sample_count;                          // 采样计数
} adc_channel_data_t;

/* ADC系统状态结构体 */
typedef struct {
    adc_channel_data_t channels[ADC_CHANNEL_COUNT]; // 各通道数据
    uint32_t total_sample_count;                    // 总采样计数
    uint32_t last_output_time;                      // 上次输出时间
    uint8_t sampling_active;                        // 采样活动标志
    uint8_t error_count;                            // 错误计数
} adc_system_data_t;

/* 外部变量声明 */
extern ADC_HandleTypeDef hadc1;
extern adc_system_data_t g_adc_system;
extern osMessageQueueId_t ADCQueueHandle;

/* 任务函数声明 */
void ADCSamplingTask(void *argument);

/* ADC功能函数声明 */
HAL_StatusTypeDef ADC_InitializeChannels(void);
HAL_StatusTypeDef ADC_SampleChannel(uint8_t channel_index);
void ADC_UpdateSlidingWindow(uint8_t channel_index, uint16_t new_value);
float ADC_CalculateVoltage(uint16_t raw_value);
void ADC_PrintChannelData(void);
void ADC_PrintSystemStatus(void);

/* 工具函数声明 */
const char* ADC_GetChannelName(uint8_t channel_index);
uint32_t ADC_GetChannelHAL(uint8_t channel_index);

/* ADC任务配置 */
#define ADC_SAMPLING_TASK_STACK_SIZE    512
#define ADC_SAMPLING_TASK_PRIORITY      osPriorityAboveNormal  // 高优先级确保精确采样

/* 任务句柄 */
extern osThreadId_t ADCSamplingTaskHandle;

/* 任务属性 */
extern const osThreadAttr_t ADCSamplingTask_attributes;

#endif // __ADC_SAMPLE_TASK_H__
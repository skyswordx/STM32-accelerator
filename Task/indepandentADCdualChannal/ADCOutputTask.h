#ifndef ADC_OUTPUT_TASK_H
#define ADC_OUTPUT_TASK_H

#include "ADCsampleTask.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* ADC输出任务相关常量 */
#define ADC_OUTPUT_TASK_STACK_SIZE 256
#define ADC_OUTPUT_TASK_PRIORITY osPriorityNormal

/* ADC输出模式枚举 */
typedef enum {
    ADC_OUTPUT_MODE_DEBUG = 0,
    ADC_OUTPUT_MODE_VOFA,
    ADC_OUTPUT_MODE_RAW_DATA,
    ADC_OUTPUT_MODE_HMI,
    ADC_OUTPUT_MODE_MAX
} adc_output_mode_t;

/* VOFA模式配置 */
#define VOFA_BUFFER_DEPTH  1024

/* HMI串口屏模式配置 */
#define HMI_SELECTED_CHANNEL  0
#define HMI_CURVE_ID  "osc"
#define HMI_CHANNEL_ID  0

/* 外部声明 */
extern osThreadId_t ADCOutputTaskHandle;
extern const osThreadAttr_t ADCOutputTask_attributes;

/* 任务函数声明 */
void ADCOutputTask(void *argument);

/* 输出处理函数声明 */
void ADC_ProcessDebugOutput(const adc_system_data_t *data);
void ADC_ProcessVofaOutput(const adc_system_data_t *data);
void ADC_ProcessRawDataOutput(const adc_system_data_t *data);
void ADC_ProcessHMIOutput(const adc_system_data_t *data);

/* 输出模式控制函数 */
void ADC_SetOutputMode(adc_output_mode_t mode);
adc_output_mode_t ADC_GetOutputMode(void);
void ADC_SwitchToNextOutputMode(void);
void ADC_PrintModeInfo(void);

#endif /* ADC_OUTPUT_TASK_H */

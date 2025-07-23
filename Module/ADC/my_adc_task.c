#include "my_adc_task.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */


#define ADC_SAMPLE_SIZE (4096)
uint32_t ADC_SAMPLE_RATE_Hz = 100000; // 100kHz采样率

uint16_t ADC_DMA_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区


void StartADCProcessingTask2(void *argument) {
    
    /* 初始化内存空间并且启动定时器和双 ADC */



    for (;;) {
        // 处理ADC数据

        
        osDelay(1); // 延时1毫秒
    }
}
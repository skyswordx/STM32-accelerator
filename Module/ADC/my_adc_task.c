#include "my_adc_task.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */

#define ADC_SAMPLE_SIZE (4096)
#define ADC_DMA_TRANSFER_COMPLETED 1
#define ADC_DMA_TRANSFER_NOT_COMPLETED 0
uint32_t g_ADC_SAMPLE_RATE_Hz = 100000; // 100kHz采样率

uint16_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
uint8_t g_adc1_data_8bit[ADC_SAMPLE_SIZE]; // ADC1数据
uint8_t g_adc2_data_8bit[ADC_SAMPLE_SIZE]; // ADC2数据

uint16_t g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if(hadc->Instance == ADC1)
  {
    /* ADC DMA 传输完成之后会进入这里 */
    HAL_TIM_Base_Stop(&htim3); // 停止定时器，停止ADC触发
    g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_COMPLETED;   
  }
}

void StartADCProcessingTask(void *argument) {
    
    /* 初始化内存空间并且启动定时器和双 ADC */
    memset(g_adc_dma_buffer, 0, ADC_SAMPLE_SIZE * sizeof(uint16_t));
    SCB_CleanDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));

    /* 校准ADC 勿动 */
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);


    /* 启动定时器3作为时间戳基准和ADC触发源 */
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE);

    HAL_TIM_Base_Start(&htim3);
    for (;;) {
        // 处理ADC数据

        /* GPIO 按键 */
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
            osDelay(10); // 防抖延时
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 切换 LED 状态
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET);

                // printf("ADC started\n");
                HAL_TIM_Base_Start(&htim3);
            }
        }
    

        if (g_adc_dma_transfer_flag == ADC_DMA_TRANSFER_COMPLETED) {
            g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED; // 重置标志
            
            /* Dcache 缓存一致性处理 */
            SCB_InvalidateDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));
            

            for (uint32_t i = 0; i < ADC_SAMPLE_SIZE; i++) {
                g_adc1_data_8bit[i] = (uint8_t)(g_adc_dma_buffer[i] & 0xFF); // ADC1数据
                g_adc2_data_8bit[i] = (uint8_t)((g_adc_dma_buffer[i] >> 8) & 0xFF); // ADC2数据

                printf("ADC1/2:%d, %d\n", g_adc1_data_8bit[i], g_adc2_data_8bit[i]);
            }
            // HAL_TIM_Base_Start(&htim3); // 重新启动定时器，继续ADC触发
        }
        
        osDelay(1); // 延时1毫秒
    }
}
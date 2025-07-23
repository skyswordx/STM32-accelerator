#include "my_adc_task.h"


extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */

#define ADC_SAMPLE_SIZE (4096)
#define ADC_DMA_TRANSFER_COMPLETED 1
#define ADC_DMA_TRANSFER_NOT_COMPLETED 0
uint32_t g_ADC_SAMPLE_RATE_Hz = 200000; // 200kHz采样率

#define ADC_REF_VOLTAGE 3.3f // ADC参考电压
#define ADC_RESOLUTION_8BIT 256.0f // 2^8=256

uint16_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
float32_t g_adc1_data_8bit[ADC_SAMPLE_SIZE]; // ADC1数据
float32_t g_adc2_data_8bit[ADC_SAMPLE_SIZE]; // ADC2数据

uint16_t g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED;

uint8_t g_sample_rate_count =0;

extern fundamental_result_t g_ch1_fundamental; // 基波结果结构
extern fundamental_result_t g_ch2_fundamental; // 基波结果结构
extern arm_cfft_radix4_instance_f32 fft_instance_radix4; // FFT实例

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

    // HAL_TIM_Base_Start(&htim3);

    for (;;) {
        // 处理ADC数据

        /* GPIO 按键 */
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
            osDelay(10); // 防抖延时
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 切换 LED 状态
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET);

                HAL_TIM_Base_Start(&htim3);     
            }
        }
    

        if (g_adc_dma_transfer_flag == ADC_DMA_TRANSFER_COMPLETED) {
            g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED; // 重置标志
            
            /* Dcache 缓存一致性处理 */
            SCB_InvalidateDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));
            

            for (uint32_t i = 0; i < ADC_SAMPLE_SIZE; i++) {
                g_adc1_data_8bit[i] = (float32_t)((g_adc_dma_buffer[i] & 0xFF) * ADC_REF_VOLTAGE / ADC_RESOLUTION_8BIT); // ADC1数据
                g_adc2_data_8bit[i] = (float32_t)(((g_adc_dma_buffer[i] >> 8) & 0xFF) * ADC_REF_VOLTAGE / ADC_RESOLUTION_8BIT); // ADC2数据
                // printf("ADC1/2:%.3f, %.3f, %lu\n", g_adc1_data_8bit[i], g_adc2_data_8bit[i], g_ADC_SAMPLE_RATE_Hz);
            }

            my_armcfft32_apply(g_adc1_data_8bit, &g_ch1_fundamental, 0, 1); // 禁用FIR和窗函数
            // my_armrfft32_apply(g_adc2_data_8bit, &g_ch1_fundamental, 1, 1); // 启用FIR和窗函数

            // my_armcfft32_apply(g_adc2_data_8bit, &g_ch2_fundamental, 0, 0);

            printf("ADC1| Freq: %d Hz, Vrms: %.6f, Phase: %.2f\n", g_ch1_fundamental.fundamental_frequency, g_ch1_fundamental.fundamental_vrms, g_ch1_fundamental.fundamental_phase_angle);
            // printf("ADC2| Freq: %d Hz, Vrms: %.6f, Phase: %.2f\n", g_ch2_fundamental.fundamental_frequency, g_ch2_fundamental.fundamental_vrms, g_ch2_fundamental.fundamental_phase_angle);
            // HAL_TIM_Base_Start(&htim3); // 重新启动定时器，继续ADC触发

            // printf("%.6f\n", g_ch1_fundamental.fundamental_vrms);
        }
        
        osDelay(1); // 延时1毫秒
    }
}
#include "my_adc_task.h"
#include "my_uart_task.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */

#define ADC_SAMPLE_SIZE (4096)
#define ADC_DMA_TRANSFER_COMPLETED 1
#define ADC_DMA_TRANSFER_NOT_COMPLETED 0
extern uint32_t g_desired_ADC_sample_rate_Hz;
uint32_t g_ADC_SAMPLE_RATE_Hz = 2000000; // 2MHz采样率

#define ADC_REF_VOLTAGE 3.3f // ADC参考电压
#define ADC_RESOLUTION_8BIT 256.0f // 2^8=256
#define ADC_RESOLUTION_10BIT 1024.0f // 2^10=1024
#define ADC_RESOLUTION_12BIT 4096.0f // 2^12=4096
#define ADC_RESOLUTION_14BIT 16384.0f // 2^14=16384
#define ADC_RESOLUTION_16BIT 65536.0f // 2^16=65536

/**
 * 1. 在 cubemx 中同时更改 2 个 ADC 的分辨率
 * 2. 在 cubemx 中更改 DMA 传输位数
 */

#define ADC_RESOLUTION 14
#if ADC_RESOLUTION == 8
    #define ADC_RESOLUTION_FACTOR ADC_RESOLUTION_8BIT
    uint16_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
    uint8_t g_right_shift = 8; // 双 ADC 8 位模式数据右移 8 位
    uint8_t g_and_mask = 0xFF; // 双 ADC 8 位模式数据掩码 8 个 1
#elif ADC_RESOLUTION == 10
    #define ADC_RESOLUTION_FACTOR ADC_RESOLUTION_10BIT
    uint32_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
    uint16_t g_right_shift = 16; // 双 ADC 10 位模式数据右移 16 位
    uint16_t g_and_mask = 0x03FF; // 双 ADC 10 位模式数据掩码 10 个 1
#elif ADC_RESOLUTION == 12
    #define ADC_RESOLUTION_FACTOR ADC_RESOLUTION_12BIT
    uint32_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
    uint16_t g_right_shift = 16; // 双 ADC 12 位模式数据右移 16 位
    uint16_t g_and_mask = 0x0FFF; // 双 ADC 12 位模式数据掩码 12 个 1
#elif ADC_RESOLUTION == 14
    #define ADC_RESOLUTION_FACTOR ADC_RESOLUTION_14BIT
    uint32_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
    uint16_t g_right_shift = 16; // 双 ADC 14 位模式数据右移 16 位
    uint16_t g_and_mask = 0x3FFF; // 双 ADC 14 位模式数据掩码 12 + 2 个 1
#elif ADC_RESOLUTION == 16
    #define ADC_RESOLUTION_FACTOR ADC_RESOLUTION_16BIT
    uint32_t g_adc_dma_buffer[ADC_SAMPLE_SIZE] __attribute__((aligned(32))); // DMA对齐缓冲区
    uint16_t g_right_shift = 16; // 双 ADC 16 位模式数据右移 16 位
    uint16_t g_and_mask = 0xFFFF; // 双 ADC 16 位模式数据掩码 16 个 1
#endif

float32_t g_adc1_data_8bit[ADC_SAMPLE_SIZE]; // ADC1数据
float32_t g_adc2_data_8bit[ADC_SAMPLE_SIZE]; // ADC2数据
// uint16_t debug1[ADC_SAMPLE_SIZE]; // 用于调试的ADC1数据
// uint16_t debug2[ADC_SAMPLE_SIZE]; // 用于调试的ADC2数据

uint16_t g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED;

uint8_t g_sample_rate_update_flag = 0;
uint8_t g_sweep_start_flag = 0; // 所有基波更新标志


extern fundamental_result_t g_ch1_fundamental; // ADC1 通道 基波结果结构
extern fundamental_result_t g_ch2_fundamental; // ADC2 通道 基波结果结构

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

    // ADC工作模式，默认为正常模式
    adc_mode_t adc_mode = ADC_MODE_SWEEP;

    for (;;) {

        // 处理ADC数据

        /* GPIO 按键 */
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
            osDelay(10); // 防抖延时
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 切换 LED 状态
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET);
                
                // 根据ADC工作模式执行相应的处理逻辑
                if (adc_mode == ADC_MODE_SWEEP) {
                    // 扫频处理逻辑
                    // 这里调用ZLCR配置中的频率点生成函数
                    // 然后设置DDS频率并采集数据
                    g_sweep_start_flag = 1; // 设置扫频开始标志
                    printf("Entering sweep mode...\n");
                } else {
                    // 原有的处理逻辑（正常模式）
                    switch_timer_sampleRate_Auto(&htim3, g_desired_ADC_sample_rate_Hz, g_desired_ADC_sample_rate_Hz / 100);
                    HAL_TIM_Base_Start(&htim3);
                }
            }
        }
    

        if (g_adc_dma_transfer_flag == ADC_DMA_TRANSFER_COMPLETED) {
            g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED; // 重置标志
            
            /* Dcache 缓存一致性处理 */
            SCB_InvalidateDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));
            
            /* 提取 ADC 数据 */
            for (uint32_t i = 0; i < ADC_SAMPLE_SIZE; i++) {
                // debug1[i] = (uint16_t)(g_adc_dma_buffer[i] & g_and_mask); // ADC1数据
                // debug2[i] = (uint16_t)((g_adc_dma_buffer[i] >> g_right_shift) & g_and_mask); // ADC2数据

                g_adc1_data_8bit[i] = (float32_t)((g_adc_dma_buffer[i] & g_and_mask) * ADC_REF_VOLTAGE / ADC_RESOLUTION_FACTOR); // ADC1数据
                g_adc2_data_8bit[i] = (float32_t)(((g_adc_dma_buffer[i] >> g_right_shift) & g_and_mask) * ADC_REF_VOLTAGE / ADC_RESOLUTION_FACTOR); // ADC2数据
                // printf("ADC1/2:%.3f, %.3f, %lu\n", g_adc1_data_8bit[i], g_adc2_data_8bit[i], g_ADC_SAMPLE_RATE_Hz);
            }

            if(adc_mode == ADC_MODE_NORMAL){
                my_armcfft32_apply(g_adc1_data_8bit, &g_ch1_fundamental, 1, 1, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值
                
                // my_armrfft32_apply(g_adc2_data_8bit, &g_ch1_fundamental, 1, 1, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值

                my_armcfft32_apply(g_adc2_data_8bit, &g_ch2_fundamental, 1, 1, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值

                printf("ADC1 %d Hz %.6f V\n", (g_ch1_fundamental.fundamental_frequency), g_ch1_fundamental.fundamental_vrms);
                printf("ADC2 %d Hz %.6f V\n", (g_ch2_fundamental.fundamental_frequency), g_ch2_fundamental.fundamental_vrms);
                printf("ADC phase angle: %.2f\n", (g_ch1_fundamental.fundamental_phase_angle - g_ch2_fundamental.fundamental_phase_angle ));
                
                // printf("ADC1| Freq: %d Hz, Vrms: %.6f, Phase: %.2f\n", g_ch1_fundamental.fundamental_frequency, g_ch1_fundamental.fundamental_vrms, g_ch1_fundamental.fundamental_phase_angle);
                // printf("ADC2| Freq: %d Hz, Vrms: %.6f, Phase: %.2f\n", g_ch2_fundamental.fundamental_frequency, g_ch2_fundamental.fundamental_vrms, g_ch2_fundamental.fundamental_phase_angle);
                // HAL_TIM_Base_Start(&htim3); // 重新启动定时器，继续ADC触发

                // printf("%.6f\n", g_ch1_fundamental.fundamental_vrms);
            }
        }

        if ( g_sweep_start_flag == 1 && adc_mode == ADC_MODE_SWEEP) {
            // 扫频模式下的处理逻辑
            // 这里可以调用ZLCR配置中的频率点生成函数
            // 然后设置DDS频率并采集数据
            
            // 示例：假设我们有一个函数来生成频率点
            // uint32_t freq_array[100];
            // generate_frequency_points(&sweep_config, freq_array);
            
            // 设置DDS频率
            // set_dds_frequency(freq_array[current_index]);
            
            // 采集数据
            // 这里可以调用ADC采样函数
            
            printf("Sweep mode processing...\n");
        }

        osDelay(100); // 延时100毫秒
    }
}
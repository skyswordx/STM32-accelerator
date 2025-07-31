#include "my_adc_task.h"
#include "my_uart_task.h"
#include "my_zlcr_config.h"  // 添加 ZLCR 配置头文件
#include "my_freq_config.h"  // 添加频率配置头文件，用于窗函数类型定义
#include "my_time_detect.h"  // 添加时域检测模块头文件
#include "AD9833.h"         // 添加 AD9833 头文件
#include "AD9954.h"         // 添加 AD9954 头文件
#include "my_parameter_config.h"  // 添加参数配置头文件
#include "main.h"           // 添加主头文件
#include <stdint.h>         // 添加标准整数类型头文件
#include <string.h>         // 添加字符串处理头文件
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


// 时域检测模块启用标志
uint8_t g_time_detect_enabled = 1; // 默认禁用


extern fundamental_result_t g_ch1_fundamental; // ADC1 通道 基波结果结构
extern fundamental_result_t g_ch2_fundamental; // ADC2 通道 基波结果结构

extern arm_cfft_radix4_instance_f32 fft_instance_radix4; // FFT实例

// 外部声明频谱分析模块中的全局变量
extern float32_t g_fft_output_buffer[FFT_LENGTH]; // FFT输出数组

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

    /* 【ADC 数据流】校准ADC 勿动 */
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);

    /* 初始化DDS设备 */
    // 在 Button Task 中调用 my_zlcr_dds_init 函数初始化 DDS 设备

    /* 【ADC 数据流】初始化同步采样的 ADC 模式 */
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE);

    // 使用这个接口 HAL_TIM_Base_Start(&htim3) 即可启动 ADC，现在不启动先，等待后续系统功能触发 

    // 【ADC 数据流】 指示 ADC 工作模式，默认为正常模式
    adc_mode_t adc_mode = ADC_MODE_NORMAL;

    for (;;) {

        /* 【ADC 数据流】 利用 GPIO 按键触发启动定时器 */
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
            osDelay(10); // 防抖延时
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 切换 LED 状态
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET);
                
                switch_timer_sampleRate_Auto(&htim3, g_desired_ADC_sample_rate_Hz, g_desired_ADC_sample_rate_Hz / 100);
                HAL_TIM_Base_Start(&htim3);
            }
        }
    

        // 【ADC 数据流】所有的 ADC 数据处理都得等待 DMA 传输完成标志位（DMA 传输完成的中断会停止 ADC 并设置标志位）
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

            if (g_time_detect_enabled) {
                time_domain_result_t ch1_time_result, ch2_time_result;

                // 对 ADC1 通道数据进行时域分析
                my_time_domain_analysis(g_adc1_data_8bit, ADC_SAMPLE_SIZE, &ch1_time_result);

                // 对 ADC2 通道数据进行时域分析
                my_time_domain_analysis(g_adc2_data_8bit, ADC_SAMPLE_SIZE, &ch2_time_result);

                // 打印时域分析结果
                printf("\r\n--- Time Domain Analysis ---\r\n");
                printf("--- Channel 1 ---\r\n");
                printf("  DC Offset     : %.6f V\r\n", ch1_time_result.dc_offset);
                printf("  AC RMS        : %.6f V\r\n", ch1_time_result.ac_rms);
                printf("  Vpp (Sine)    : %.6f V (Calculated from RMS)\r\n", ch1_time_result.vpp_sine);
                printf("  Vpp (Square)  : %.6f V (Calculated from RMS)\r\n", ch1_time_result.vpp_square);
                printf("  Vpp (Peak-Peak) : %.6f V (Vmax - Vmin)\r\n", ch1_time_result.vpp_peak);
                printf("  Vmax          : %.6f V, Vmin: %.6f V\r\n", ch1_time_result.v_max, ch1_time_result.v_min);
                
                printf("--- Channel 2 ---\r\n");
                printf("  DC Offset     : %.6f V\r\n", ch2_time_result.dc_offset);
                printf("  AC RMS        : %.6f V\r\n", ch2_time_result.ac_rms);
                printf("  Vpp (Sine)    : %.6f V (Calculated from RMS)\r\n", ch2_time_result.vpp_sine);
                printf("  Vpp (Square)  : %.6f V (Calculated from RMS)\r\n", ch2_time_result.vpp_square);
                printf("  Vpp (Peak-Peak) : %.6f V (Vmax - Vmin)\r\n", ch2_time_result.vpp_peak);
                printf("  Vmax          : %.6f V, Vmin: %.6f V\r\n", ch2_time_result.v_max, ch2_time_result.v_min);
                printf("--------------------------\r\n\r\n");
            }

            fundamental_result_t ch1_result, ch2_result;
                        
            // 分别计算两个通道的频谱数据并存储到独立的缓冲区中
            my_armcfft32_apply(g_adc1_data_8bit, &ch1_result, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
            memcpy(g_adc1_spectrum_data, g_fft_output_buffer, (FFT_LENGTH / 2) * sizeof(float32_t));
                        
            my_armcfft32_apply(g_adc2_data_8bit, &ch2_result, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
            memcpy(g_adc2_spectrum_data, g_fft_output_buffer, (FFT_LENGTH / 2) * sizeof(float32_t));
                        
            // 打印频谱

            // 打印频谱分析结果
            printf("=== Spectrum Analysis Results ===\n");
            printf("ADC1 - Frequency: %lu Hz, Magnitude: %.6f V\n", ch1_result.fundamental_frequency, ch1_result.fundamental_vpp);
            printf("ADC2 - Frequency: %lu Hz, Magnitude: %.6f V\n", ch2_result.fundamental_frequency, ch2_result.fundamental_vpp);

            switch (g_desired_function_state) {
                case SPECTRUM_STATE:
                    printf("=== Spectrum Results ===\n");
                        for (uint32_t i = 0; i < (FFT_LENGTH / 2); i++) {
                            printf("  Frequency Bin %lu: %.6f V\n", i, g_adc1_spectrum_data[i]);
                        }
                        for (uint32_t i = 0; i < (FFT_LENGTH / 2); i++) {
                            printf("  Frequency Bin %lu: %.6f V\n", i, g_adc2_spectrum_data[i]);
                        }
                    break;
                case TIME_STATE:
                    printf("=== Time Domain Analysis Results ===\n");
                    
                    // 打印ADC1和ADC2的时域数据
                    for (uint32_t i = 0; i < ADC_SAMPLE_SIZE; i++) { 
                        printf("raw ADC1/2 :%.6f,%.6f\n", g_adc1_data_8bit[i], g_adc2_data_8bit[i]);

                    }
                    break;
                }
        }
       osDelay(100); // 延时100毫秒
    }
}
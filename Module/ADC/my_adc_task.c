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

extern sweep_point_result_t g_current_freq_result;

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

                    /* 在这里使用一个在 my_zlcr_config.c 的接口生成扫频点 */
                    // 参数：起始频率
                    // 参数：终止频率
                    // 参数：对数扫频/十倍频扫描/线性扫频


                    /* 在这里利用初始化 DDS */
                    // 这里可以使用函数指针和结构体，在 my_zlcr_config.h 中定义一个 DDS 配置结构体，成员是抽象出来的 DDS 初始化函数指针。
                    // 用于在当前文件绑定实际使用的 DDS：9833

                    /* 使用 DDS 输出扫频数组中第一个频率点 */

                    /* 等待 DDS 输出稳定之后，用 timer3 启动定时器开启 ADC 采样 */

                } else if (adc_mode == ADC_MODE_NORMAL) {
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

            if (adc_mode == ADC_MODE_SWEEP) {
                // 扫频模式下
                /* 在一段频率中，每一个频率点的 ADC 数据都会被采集 */
                /* 进入这个部分，意味着当前频率点的采集已经完成 */
                // 特别地，第一个频率点在按键按下那会就触发 ADC 采样了，此时第一次进来这里对应的就是第一个频率点的采集完成

                /* 需要调用 my_armcfft32_apply 函数进行频域分析 */
                my_armcfft32_apply(g_adc1_data_8bit, &g_ch1_fundamental, 1, 1, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值
                my_armcfft32_apply(g_adc2_data_8bit, &g_ch2_fundamental, 1, 1, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值

                /* 然后利用频域分析结果 g_ch1_fundamental 和 g_ch2_fundamental ，利用 my_zlrc_config 中的接口，得到当前频率下的阻抗信息 */
                my_zlrc_get_impedance(&g_ch1_fundamental, &g_ch2_fundamental, &g_current_freq_result); // 获取当前频率下的阻抗信息

                /* 为了绘制扫频得到的幅频和相频特性，需要将结果保存到对应的数组中 */
                // 这里可以将 g_current_freq_result 中的幅值和相位保存到，对应的全局数组中（要在 my_zlcr_config.c 中定义、在当前文件 extern）
                // 全局数组的大小有多种选择，分别对应于不同的测量场景（如 多10倍频扫描、测电阻模式、测电容、测电感模式）


                /* 存储完当前频率点测得的未知阻抗信息之后，要切换 DDS 的输出信号频率，测量下一个频率点的阻抗特性 */
                // 这里可以使用函数指针和结构体，在 my_zlcr_config.h 中定义一个 DDS 配置结构体，成员是抽象出来的 DDS 设置输出频率波形的函数指针。
                // 用于在当前文件绑定实际使用的 DDS：9833
                // 用 DDS 设置下一个频率，带输出稳定之后，利用 timer3 触发 ADC 采样，采集下一个频率点的 ADC 数据

                /* 之后依次存储 g_current_freq_result 中的幅值和相位到对应的全局数组中。直到数组存满，扫频完成，打印输出本次的结果数组 */
                // 扫频完成后，就不再使用 timer3 触发 ADC 了
            }
        }
       osDelay(100); // 延时100毫秒
    }
}
#include "my_adc_task.h"
#include "my_uart_task.h"
#include "my_zlcr_config.h"  // 添加 ZLCR 配置头文件
#include "my_freq_config.h"  // 添加频率配置头文件，用于窗函数类型定义
#include "my_time_detect.h"  // 添加时域检测模块头文件
#include <stdint.h>          // 确保包含标准整数类型定义
#include "my_time_detect.h"  // 添加时域检测模块头文件
#include "AD9833.h"         // 添加 AD9833 头文件
#include "AD9954.h"         // 添加 AD9954 头文件

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
uint8_t g_sweep_start_flag = 0; // 扫频开始标志
uint8_t g_sweep_in_progress = 0; // 扫频进行中标志

// 时域检测模块启用标志
uint8_t g_time_detect_enabled = 1; // 默认禁用

// 扫频配置参数
#define SWEEP_START_FREQ 1000    // 起始频率 1kHz
#define SWEEP_STOP_FREQ 100000  // 终止频率 100kHz
#define SWEEP_POINTS 100         // 扫频点数

extern fundamental_result_t g_ch1_fundamental; // ADC1 通道 基波结果结构
extern fundamental_result_t g_ch2_fundamental; // ADC2 通道 基波结果结构

extern arm_cfft_radix4_instance_f32 fft_instance_radix4; // FFT实例

extern sweep_point_result_t g_current_freq_result;

// 外部声明 ZLCR 配置中的全局变量
extern uint32_t g_sweep_freq_array[SWEEP_MAX_POINTS];
extern sweep_point_result_t g_sweep_result_array[SWEEP_MAX_POINTS];
extern uint32_t g_sweep_current_index;
extern uint32_t g_sweep_total_points;

extern impedance_result_t g_current_impedance_result; // 当前频率下的阻抗结果

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

    /* 初始化DDS设备 */
    my_zlcr_dds_init(DDS_TYPE_AD9833);

    /* 启动定时器3作为时间戳基准和ADC触发源 */
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE);

    // HAL_TIM_Base_Start(&htim3);

    // ADC工作模式，默认为正常模式
    adc_mode_t adc_mode = ADC_MODE_NORMAL;

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
                    printf("Entering sweep mode...\n");
                    
                    // 初始化扫频配置
                    sweep_config_t sweep_config;
                    sweep_config.start_freq = SWEEP_START_FREQ;
                    sweep_config.stop_freq = SWEEP_STOP_FREQ;
                    sweep_config.points = SWEEP_POINTS;
                    sweep_config.mode = SWEEP_MODE_LOG;  // 使用对数扫频
                    sweep_config.points_per_decade = 10; // 每十倍频10个点
                    
                    // 生成扫频点
                    my_zlcr_sweep_init(&sweep_config);
                    
                    // 开始扫频
                    my_zlcr_sweep_start();
                    
                    // 设置扫频进行中标志
                    g_sweep_in_progress = 1;
                    
                    // 等待DDS输出稳定(约10ms)
                    osDelay(10);
                    
                    // 启动定时器开启ADC采样
                    switch_timer_sampleRate_Auto(&htim3, g_desired_ADC_sample_rate_Hz, g_desired_ADC_sample_rate_Hz / 100);
                    HAL_TIM_Base_Start(&htim3);
                    
                    printf("Sweep started, total points: %lu\n", g_sweep_total_points);
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
                my_armcfft32_apply(g_adc1_data_8bit, &g_ch1_fundamental, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值
                
                // my_armrfft32_apply(g_adc2_data_8bit, &g_ch1_fundamental, 1, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值

                my_armcfft32_apply(g_adc2_data_8bit, &g_ch2_fundamental, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值

                printf("ADC1 %d Hz %.6f V\n", (g_ch1_fundamental.fundamental_frequency), g_ch1_fundamental.fundamental_vrms);
                printf("ADC2 %d Hz %.6f V\n", (g_ch2_fundamental.fundamental_frequency), g_ch2_fundamental.fundamental_vrms);
                printf("ADC phase angle: %.2f\n", (g_ch1_fundamental.fundamental_phase_angle - g_ch2_fundamental.fundamental_phase_angle ));
                
                my_zlcr_get_impedance(&g_ch1_fundamental, &g_ch2_fundamental, &g_current_freq_result); // 获取当前频率下的阻抗信息
                printf("Frequency: %d Hz, Impedance: %.2f Ohm, Phase: %.2f deg\n", g_current_freq_result.frequency, g_current_freq_result.magnitude, g_current_freq_result.phase);
                
                my_zlcr_get_capacitance_or_inductance(&g_current_freq_result, &g_current_impedance_result); // 获取电容或电感信息
                
                // 如果启用了时域检测模块，则进行检测
                if (g_time_detect_enabled) {
                    // 初始化时域检测配置
                    time_detect_config_params_t time_config;
                    time_config.sample_rate = g_ADC_SAMPLE_RATE_Hz;
                    time_config.data_length = ADC_SAMPLE_SIZE;
                    time_config.enable_dc_filter = 1; // 启用直流分量滤除
                    
                    // 初始化时域检测模块
                    my_time_detect_init(&time_config);
                    
                    // 启动时域检测（内部使用频域处理模块获取基波频率等参数）
                    time_detect_result_t result;
                    if (my_time_detect_start(g_adc1_data_8bit, &result) == 0) {
                        // 输出检测结果
                        printf("Time Domain Detection Results:\n");
                        printf("  DC Component: %.6f V\n", result.dc_component);
                        printf("  RMS Value: %.6f V\n", result.rms_value);
                        printf("  Fundamental Frequency: %lu Hz\n", result.fundamental_freq);
                        printf("  Period Points: %lu\n", result.period_points);
                        printf("  Waveform Ratio: %.6f\n", result.waveform_ratio);
                        
                        // 根据波形类型输出
                        switch (result.waveform_type) {
                            case WAVEFORM_SINE:
                                printf("  Waveform Type: Sine Wave\n");
                                break;
                            case WAVEFORM_SQUARE:
                                printf("  Waveform Type: Square Wave\n");
                                break;
                            case WAVEFORM_TRIANGLE:
                                printf("  Waveform Type: Triangle Wave\n");
                                break;
                            default:
                                printf("  Waveform Type: Unknown\n");
                                break;
                        }
                    } else {
                        printf("Time domain detection failed!\n");
                    }
                }
                
            }

            if (adc_mode == ADC_MODE_SWEEP && g_sweep_in_progress) {
                // 扫频模式下
                /* 在一段频率中，每一个频率点的 ADC 数据都会被采集 */
                /* 进入这个部分，意味着当前频率点的采集已经完成 */

                /* 需要调用 my_armcfft32_apply 函数进行频域分析 */
                my_armcfft32_apply(g_adc1_data_8bit, &g_ch1_fundamental, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值
                my_armcfft32_apply(g_adc2_data_8bit, &g_ch2_fundamental, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL); // 启用FIR和窗函数，并使用汉宁窗专用插值

                /* 然后利用频域分析结果 g_ch1_fundamental 和 g_ch2_fundamental ，利用 my_zlrc_config 中的接口，得到当前频率下的阻抗信息 */
                my_zlcr_get_impedance(&g_ch1_fundamental, &g_ch2_fundamental, &g_current_freq_result); // 获取当前频率下的阻抗信息

                /* 为了绘制扫频得到的幅频和相频特性，需要将结果保存到对应的数组中 */
                if (g_sweep_current_index < SWEEP_MAX_POINTS) {
                    g_sweep_result_array[g_sweep_current_index] = g_current_freq_result;
                }

                /* 检查扫频是否完成 */
                if (my_zlcr_sweep_is_complete()) {
                    // 扫频完成
                    g_sweep_in_progress = 0;
                    printf("Sweep completed! Printing results:\n");
                    
                    // 打印扫频结果
                    for (uint32_t i = 0; i < g_sweep_total_points; i++) {
                        printf("Freq: %lu Hz, Magnitude: %.2f Ohm, Phase: %.2f deg\n",
                               g_sweep_result_array[i].frequency,
                               g_sweep_result_array[i].magnitude,
                               g_sweep_result_array[i].phase);
                    }
                } else {
                    /* 存储完当前频率点测得的未知阻抗信息之后，要切换 DDS 的输出信号频率，测量下一个频率点的阻抗特性 */
                    my_zlcr_sweep_next();
                    
                    // 等待DDS输出稳定(约10ms)
                    osDelay(10);
                    
                    /* 用 timer3 启动定时器开启 ADC 采样 */
                    switch_timer_sampleRate_Auto(&htim3, g_desired_ADC_sample_rate_Hz, g_desired_ADC_sample_rate_Hz / 100);
                    HAL_TIM_Base_Start(&htim3);
                }
            }
        }
       osDelay(100); // 延时100毫秒
    }
}
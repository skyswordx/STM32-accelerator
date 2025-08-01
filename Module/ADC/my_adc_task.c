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
#include "my_dds.h"          // 添加 DDS 头文件

// --- 新增: 包含滤波器辨识模块和数学库 ---
#include "filter_identification.h"
#include "arm_math.h" // 确保arm_math.h已包含

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */

#define ADC_SAMPLE_SIZE (4096)
#define ADC_DMA_TRANSFER_COMPLETED 1
#define ADC_DMA_TRANSFER_NOT_COMPLETED 0
extern uint32_t g_desired_ADC_sample_rate_Hz;
uint32_t g_ADC_SAMPLE_RATE_Hz = 400000; // 默认采样率 400kHz
// uint32_t g_ADC_SAMPLE_RATE_Hz = 995062*2; // 2MHz采样率
// uint32_t g_ADC_SAMPLE_RATE_Hz = 2000000; // 2MHz采样率

#define ADC_REF_VOLTAGE 3.3f // ADC参考电压
#define ADC_RESOLUTION_8BIT 256.0f // 2^8=256
#define ADC_RESOLUTION_10BIT 1024.0f // 2^10=1024
#define ADC_RESOLUTION_12BIT 4096.0f // 2^12=4096
#define ADC_RESOLUTION_14BIT 16384.0f // 2^14=16384
#define ADC_RESOLUTION_16BIT 65536.0f // 2^16=65536

// 扫频参数 (必须与 filter_identification.h 中 NUM_FREQ_POINTS 的计算方式保持一致)
#define SWEEP_F_START_HZ 1000.0f
#define SWEEP_F_STOP_HZ  50000.0f
#define SWEEP_F_STEP_HZ  100.0f

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

// --- 新增: 为时域分析提供静态临时缓冲区 ---
float32_t g_adc1_temp_buffer[ADC_SAMPLE_SIZE];
float32_t g_adc2_temp_buffer[ADC_SAMPLE_SIZE];

uint16_t g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED;


// 时域检测模块启用标志
uint8_t g_time_detect_enabled = 0; // 默认禁用


extern DDS_Generator_t g_dds_generator; // 引用全局DDS生成器实例

// 用于存储扫频数据的静态数组，防止堆栈溢出
static float32_t g_sweep_w_rad[NUM_FREQ_POINTS];             // 角频率 (rad/s)
static float32_t g_sweep_H_cmplx[NUM_FREQ_POINTS * 2];       // 复数响应 [R,I,R,I...]
static ContinuousTransferFunction g_identified_tf;         // 存储辨识结果
static uint32_t g_sweep_step = 0;                          // 当前扫频步数

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

// ADC 工作模式，默认为空闲
static adc_mode_t g_adc_mode = ADC_MODE_IDLE;



void StartADCProcessingTask(void *argument) {
    
    /* 初始化内存空间并且启动定时器和双 ADC */
    memset(g_adc_dma_buffer, 0, ADC_SAMPLE_SIZE * sizeof(uint16_t));
    SCB_CleanDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));

    /* 【ADC 数据流】校准ADC 勿动 */
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);

    /* 【ADC 数据流】初始化同步采样的 ADC 模式 */
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE);

    // --- 新增: 初始化时域分析的配置 ---
    time_domain_config_t time_config;
    time_config.enable_filter = 0;         // 启用滤波器
    time_config.filter_alpha = 0.05f;      // 设置IIR滤波器系数
    time_config.hysteresis_v = 0.05f;      // 设置50mV的迟滞电压窗口

    printf("System Initialized. Press User Button (PC1) to start Filter Identification Sweep.\r\n");


    for (;;) {

        /* 【ADC 数据流】 利用 GPIO 按键触发启动定时器 */
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
            osDelay(10); // 防抖延时
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET){
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 切换 LED 状态
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET);
                
                printf("\r\n--- Starting Filter Identification Sweep ---\r\n");
                g_adc_mode = ADC_MODE_SWEEP;
                g_sweep_step = 0;

                // 设置DDS到起始频率
                float32_t current_freq_hz = SWEEP_F_START_HZ + g_sweep_step * SWEEP_F_STEP_HZ;
                AD9954_Set_Fre(current_freq_hz); // 假设使用AD9954
                // DDS_SetFrequency(&g_dds_generator, current_freq_hz); // 使用 DAC + 软件 DDS

                printf("Step %lu/%d: Freq = %.1f Hz\r\n", g_sweep_step + 1, NUM_FREQ_POINTS, current_freq_hz);
                
                // 启动ADC采样
                // switch_timer_sampleRate_Auto(&htim3, g_desired_ADC_sample_rate_Hz, g_desired_ADC_sample_rate_Hz / 100);
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

              // --- 核心逻辑: 处理扫频数据 ---
            if (g_adc_mode == ADC_MODE_SWEEP) {
                
                // 1. 对两个通道进行FFT分析
                fundamental_result_t ch1_res, ch2_res;
                float32_t current_freq_hz = SWEEP_F_START_HZ + g_sweep_step * SWEEP_F_STEP_HZ;

                // 分析通道1 (输入)
                my_armcfft32_apply(g_adc1_data_8bit, &ch1_res, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
                
                // 分析通道2 (输出)
                my_armcfft32_apply(g_adc2_data_8bit, &ch2_res, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
 
                
                // 2. 计算复数传递函数 H(jω)
                float32_t h_mag, h_phase, h_real, h_imag;

                // 幅度比
                if (ch1_res.fundamental_vpp < 1e-6) { // 防止除以零
                    h_mag = 0.0f;
                } else {
                    h_mag = ch2_res.fundamental_vpp / ch1_res.fundamental_vpp;
                }
                
                // 相位差
                h_phase = ch1_res.fundamental_phase - ch2_res.fundamental_phase;
                
                // 3. 转换为笛卡尔坐标系 (实部和虚部)
                h_real = h_mag * arm_cos_f32(h_phase);
                h_imag = h_mag * arm_sin_f32(h_phase);
                
                // 4. 存储结果
                g_sweep_w_rad[g_sweep_step] = 2.0f * PI * current_freq_hz;
                g_sweep_H_cmplx[g_sweep_step * 2]     = h_real;
                g_sweep_H_cmplx[g_sweep_step * 2 + 1] = h_imag;

                // 5. 进入下一步或结束扫频
                g_sweep_step++;
                if (g_sweep_step < NUM_FREQ_POINTS) {
                    // 设置DDS到下一步频率
                    current_freq_hz = SWEEP_F_START_HZ + g_sweep_step * SWEEP_F_STEP_HZ;
                    AD9954_Set_Fre(current_freq_hz);
                    printf("Step %lu/%d: Freq = %.1f Hz, H_mag=%.4f, H_phase=%.2f deg\r\n", 
                           g_sweep_step, NUM_FREQ_POINTS, current_freq_hz, h_mag, h_phase * 180.0f / PI);
                    
                    // 延时一小段时间以等待DDS和被测电路稳定
                    osDelay(50);
                    
                    // 启动下一次ADC采样
                    HAL_TIM_Base_Start(&htim3);

                } else {
                    // 扫频完成
                    printf("\r\n--- Sweep Finished. Running Identification Algorithm... ---\r\n");
                    
                    // 调用辨识函数
                    identify_filter(&g_identified_tf, g_sweep_w_rad, g_sweep_H_cmplx);

                    // 打印辨识结果
                    if (isnan(g_identified_tf.b0)) {
                        printf("-> ERROR: Identification failed. Matrix solution might have failed.\r\n");
                    } else {
                        const char* type_str[] = {"LPF", "HPF", "BPF", "BSF", "Unknown"};
                        printf("-> Identified Filter Type: %s\r\n", type_str[g_identified_tf.identified_type]);
                        printf("-> H(s) = (b2*s^2 + b1*s + b0) / (s^2 + a1*s + a0)\r\n");
                        printf("-> Identified Coefficients:\r\n");
                        printf("   b2 = %e\r\n", g_identified_tf.b2);
                        printf("   b1 = %e\r\n", g_identified_tf.b1);
                        printf("   b0 = %e\r\n", g_identified_tf.b0);
                        printf("   a1 = %e\r\n", g_identified_tf.a1);
                        printf("   a0 = %e\r\n", g_identified_tf.a0);
                    }
                    
                    printf("\r\nIdentification complete. System is now idle.\r\n");
                    g_adc_mode = ADC_MODE_IDLE; // 返回空闲模式
                }
            }

            if (g_time_detect_enabled) {
                time_domain_result_t ch1_time_result, ch2_time_result;

                // --- 调用优化的时域分析函数 ---
                my_time_domain_analysis_optimized(g_adc1_data_8bit, g_adc1_temp_buffer, ADC_SAMPLE_SIZE, g_ADC_SAMPLE_RATE_Hz, &time_config, &ch1_time_result);
                my_time_domain_analysis_optimized(g_adc2_data_8bit, g_adc2_temp_buffer, ADC_SAMPLE_SIZE, g_ADC_SAMPLE_RATE_Hz, &time_config, &ch2_time_result);

                // --- 打印优化后的分析结果 ---
                printf("\r\n--- Optimized Time Domain Analysis ---\r\n");
                printf("--- Channel 1 ---\r\n");
                printf("  Frequency     : %.3f Hz\r\n", ch1_time_result.frequency);
                printf("  Vpp (Peak-Peak) : %.4f V\r\n", ch1_time_result.vpp_peak);
                printf("  AC RMS        : %.4f V\r\n", ch1_time_result.ac_rms);
                printf("  DC Offset     : %.4f V\r\n", ch1_time_result.dc_offset);
                printf("  V Max        : %.4f V\r\n", ch1_time_result.v_max);
                printf("  V Min        : %.4f V\r\n", ch1_time_result.v_min);
                
                printf("--- Channel 2 ---\r\n");
                printf("  Frequency     : %.3f Hz\r\n", ch2_time_result.frequency);
                printf("  Vpp (Peak-Peak) : %.4f V\r\n", ch2_time_result.vpp_peak);
                printf("  AC RMS        : %.4f V\r\n", ch2_time_result.ac_rms);
                printf("  DC Offset     : %.4f V\r\n", ch2_time_result.dc_offset);
                printf("  V Max        : %.4f V\r\n", ch2_time_result.v_max);
                printf("  V Min        : %.4f V\r\n", ch2_time_result.v_min);

                printf("-------------------------------------\r\n\r\n");
                fundamental_result_t ch1_freq_result, ch2_freq_result;

                // 分别计算两个通道的频谱数据并存储到独立的缓冲区中
                my_armcfft32_apply(g_adc1_data_8bit, &ch1_freq_result, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
                memcpy(g_adc1_spectrum_data, g_fft_output_buffer, (FFT_LENGTH / 2) * sizeof(float32_t));

                my_armcfft32_apply(g_adc2_data_8bit, &ch2_freq_result, 0, WINDOW_HANNING, INTERPOLATION_HANNING_SPECIAL);
                memcpy(g_adc2_spectrum_data, g_fft_output_buffer, (FFT_LENGTH / 2) * sizeof(float32_t));
                            
                // 打印频谱

                // 打印频谱分析结果
                printf("=== Spectrum Analysis Results ===\n");
                printf("ADC1 - Frequency: %lu Hz, Magnitude: %.6f V\n", ch1_freq_result.fundamental_frequency, ch1_freq_result.fundamental_vpp);
                printf("ADC2 - Frequency: %lu Hz, Magnitude: %.6f V\n", ch2_freq_result.fundamental_frequency, ch2_freq_result.fundamental_vpp);
            }

            

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
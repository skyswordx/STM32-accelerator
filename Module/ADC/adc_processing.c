/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : adc_processing.c
  * @brief          : ADC sampling and frequency domain processing functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "adc_processing.h"
#include "string.h"
#include "cmsis_os.h"  /* Add CMSIS RTOS header for osKernelGetTickCount functions */

/* 定时器句柄外部引用 */
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源 */

/* Private defines -----------------------------------------------------------*/
/* 系统参数配置 */
#define DEFAULT_SAMPLING_RATE    10730000.0f  // 默认采样率 10.73 MHz
#define DEFAULT_SHI_COEFFICIENT  0.09f        // 默认神秘系数
#define TIME_DOMAIN_SAMPLE_INTERVAL_US  50    // 时域采样间隔(微秒)
#define ADC_REFERENCE_VOLTAGE    3.3f         // ADC参考电压
/* Private types -------------------------------------------------------------*/
/* ADC处理配置结构 */
typedef struct {
    float sampling_rate;      // 采样率 (Hz)
    float shi_coefficient;    // 神秘系数
    uint8_t remove_dc;        // 是否滤除直流分量
    uint32_t fft_update_interval_ms;  // FFT更新间隔 (毫秒)
} adc_processing_config_t;

/* Private variables ---------------------------------------------------------*/
/* 采样数据缓冲区 */
#if USE_DUAL_ADC_INTERLEAVED
uint16_t merged_adc_data[ADC_BUFFER_SIZE * 2]; // 交错模式合并数据
#endif

#if USE_DUAL_ADC_SIMULTANEOUS
uint8_t adc1_data_8bit[ADC_BUFFER_SIZE]; // ADC1同步采样数据
uint8_t adc2_data_8bit[ADC_BUFFER_SIZE]; // ADC2同步采样数据
#endif

/* DMA和控制变量 */
volatile uint8_t adc_conversion_complete = 0;
volatile uint8_t buffer_swap_flag = 0;

/* 双缓冲机制 */
uint16_t dmabuffer_ping[ADC_BUFFER_SIZE] __attribute__((aligned(32)));
uint16_t dmabuffer_pong[ADC_BUFFER_SIZE] __attribute__((aligned(32)));
uint16_t* active_dma_buffer = dmabuffer_ping;
uint16_t* processing_buffer = dmabuffer_pong;

/* FFT处理变量 */
arm_cfft_radix4_instance_f32 scfft;

/* 交替采样模式的FFT数据结构 */
#if USE_DUAL_ADC_INTERLEAVED
  float merged_fft_inputbuf[2 * FFT_LENGTH];    // 交替采样FFT输入数组（复数形式）
  float merged_magnitude_array[FFT_LENGTH/2];    // 交替采样幅度谱数组

/* 同步采样模式的FFT数据结构 */
#elif USE_DUAL_ADC_SIMULTANEOUS
  float adc1_fft_inputbuf[2 * FFT_LENGTH];      // ADC1 FFT输入数组（复数形式）
  float adc2_fft_inputbuf[2 * FFT_LENGTH];      // ADC2 FFT输入数组（复数形式）
  float adc1_magnitude_array[FFT_LENGTH/2];      // ADC1幅度谱数组
  float adc2_magnitude_array[FFT_LENGTH/2];      // ADC2幅度谱数组
#endif

/* 处理配置 */
static adc_processing_config_t processing_config = {
    .sampling_rate = DEFAULT_SAMPLING_RATE,
    .shi_coefficient = DEFAULT_SHI_COEFFICIENT,
    .remove_dc = 1,
    .fft_update_interval_ms = 1000
};

/* 时间控制变量 */
uint32_t last_fft_update_time = 0;
uint32_t time_domain_sample_index = 0;

/* ADC采样控制变量 */
uint32_t buffer_fill_count = ADC_DEFAULT_BUFFER_COUNT;       // 已采集的缓冲区数量
uint32_t max_buffer_fill_count = ADC_DEFAULT_MAX_BUFFER_COUNT; // 最大采集缓冲区数量，可配置
uint8_t adc_sampling_active = ADC_SAMPLING_ACTIVE_DEFAULT;   // ADC采样状态标志
uint8_t auto_stop_enabled = ADC_AUTO_STOP_ENABLED;           // 自动停止采样使能标志

/* Private function prototypes -----------------------------------------------*/
/* 底层数据处理函数 */
void ExtractDualADCData(uint16_t* dma_buffer);  // Made public for task access
void PrepareFFTInput(uint16_t* adc_data, uint32_t start_index, uint32_t data_length, float* fft_buffer);
void ExecuteFFTAndBuildSpectrum(float* fft_buffer, float* magnitude_buffer);

/* 数据输出函数 */
void OutputTimeDomainData(void);
void OutputFrequencySpectrum(void);

/* 工具函数 */
float CalculateDCComponent(uint16_t* data, uint32_t length);

/* Functions implementation --------------------------------------------------*/

/**
 * @brief 设置ADC处理配置参数
 * @param sampling_rate 采样率 (Hz)
 * @param shi_coefficient 神秘系数
 * @param remove_dc 是否滤除直流分量
 * @param fft_update_interval_ms FFT更新间隔 (毫秒)
 * @retval None
 */
void ADC_Processing_SetConfig(float sampling_rate, float shi_coefficient, 
                              uint8_t remove_dc, uint32_t fft_update_interval_ms)
{
  processing_config.sampling_rate = sampling_rate;
  processing_config.shi_coefficient = shi_coefficient;
  processing_config.remove_dc = remove_dc;
  processing_config.fft_update_interval_ms = fft_update_interval_ms;
}

/**
 * @brief 设置最大采样缓冲区数量
 * @param count 最大采样缓冲区数量
 * @retval None
 */
void ADC_Processing_SetMaxBufferCount(uint32_t count)
{
  if (count > 0) {
    max_buffer_fill_count = count;
  }
}

/**
 * @brief 获取当前ADC处理配置
 * @retval adc_processing_config_t* 配置结构体指针
 */
const adc_processing_config_t* ADC_Processing_GetConfig(void)
{
  return &processing_config;
}

/**
 * @brief 交换DMA缓冲区
 * @retval None
 */
void SwapDMABuffers(void)
{
  #if USE_DUAL_ADC_INTERLEAVED || USE_DUAL_ADC_SIMULTANEOUS
    /* 停止当前DMA传输 */
    HAL_ADCEx_MultiModeStop_DMA(&hadc1);
  #endif
  
  /* 交换缓冲区指针 */
  uint16_t* temp = active_dma_buffer;
  active_dma_buffer = processing_buffer;
  processing_buffer = temp;
  
  #if USE_DUAL_ADC_INTERLEAVED || USE_DUAL_ADC_SIMULTANEOUS
    /* 重新启动DMA传输到新的活动缓冲区 */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
  #endif
}

/**
 * @brief 处理完整的缓冲区数据 - 重构为清晰的数据流
 * @param buffer 要处理的缓冲区指针
 * @retval None
 */
void ProcessCompleteBuffer(uint16_t* buffer)
{
  /* 步骤1: 缓存一致性处理 */
  SCB_InvalidateDCache_by_Addr((uint32_t*)buffer, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  /* 步骤2: 数据提取和转换 */
  ExtractDualADCData(buffer);
  
  /* 步骤3: 时域数据输出 */
  // OutputTimeDomainData();
  
  /* 步骤4: 频域处理 (按配置的时间间隔执行) */
  uint32_t current_time = osKernelGetTickCount();  // 使用 CMSIS RTOS 时间戳
  uint32_t tick_freq_ms = 1000 / osKernelGetTickFreq();  // 计算每个tick的毫秒数
  if ((current_time - last_fft_update_time) * tick_freq_ms >= processing_config.fft_update_interval_ms) 
  {
    last_fft_update_time = current_time;
    #if USE_DUAL_ADC_INTERLEAVED
      /* 起始索引设为0，从头开始处理 */
      ADC_Processing_TriggerFFT(merged_adc_data, 0, ADC_BUFFER_SIZE * 2, 
                               merged_fft_inputbuf, merged_magnitude_array);
    #elif USE_DUAL_ADC_SIMULTANEOUS
      /* 处理ADC1数据 - 起始索引设为0，从头开始处理 */
      ADC_Processing_TriggerFFT((uint16_t*)adc1_data_8bit, 0, ADC_BUFFER_SIZE, 
                               adc1_fft_inputbuf, adc1_magnitude_array);
      
      /* 处理ADC2数据 - 起始索引设为0，从头开始处理 */
      ADC_Processing_TriggerFFT((uint16_t*)adc2_data_8bit, 0, ADC_BUFFER_SIZE, 
                               adc2_fft_inputbuf, adc2_magnitude_array);
    #endif
  }
  OutputFrequencySpectrum();
  
  /* 步骤5: 更新缓冲区计数 */
  buffer_fill_count++;
}

/* =============================================================================
 * 私有函数实现 - 重构后的模块化功能
 * ============================================================================= */

/**
 * @brief 从DMA缓冲区提取双ADC数据
 * @param dma_buffer DMA缓冲区指针
 * @retval None
 */
void ExtractDualADCData(uint16_t* dma_buffer)
{
  #if USE_DUAL_ADC_INTERLEAVED 
    /* 交错模式：解包DMA数据到交替数组
     * dma_buffer[j] = 0xXXYY (16位)
     * XX (高8位) = ADC2 (slave), YY (低8位) = ADC1 (master)
     * 交替排列：merged_data[0] = ADC1[0], merged_data[1] = ADC2[0]...
     */
    for(uint32_t j = 0; j < ADC_BUFFER_SIZE; j++) {
      merged_adc_data[j * 2] = (uint16_t)(dma_buffer[j] & 0xFF);        // ADC1数据
      merged_adc_data[j * 2 + 1] = (uint16_t)((dma_buffer[j] >> 8) & 0xFF); // ADC2数据
    }
    
  #elif USE_DUAL_ADC_SIMULTANEOUS
    /* 同步模式：分离ADC1和ADC2数据到独立数组 */
    for (uint32_t j = 0; j < ADC_BUFFER_SIZE; j++) {
      adc1_data_8bit[j] = (uint8_t)(dma_buffer[j] & 0xFF);      // ADC1数据
      adc2_data_8bit[j] = (uint8_t)((dma_buffer[j] >> 8) & 0xFF); // ADC2数据
    }
  #endif
}

/**
 * @brief 准备FFT输入数据 - 支持任意段处理
 * @param adc_data ADC数据缓冲区
 * @param start_index 开始处理的数据索引 (从adc_data[start_index]开始)
 * @param data_length 数据总长度 (用于边界检查)
 * @param fft_buffer 目标FFT缓冲区 (大小需为2*FFT_LENGTH)
 * @retval None
 */
void PrepareFFTInput(uint16_t* adc_data, uint32_t start_index, uint32_t data_length, float* fft_buffer)
{
  /* 确保索引不越界 */
  if (start_index >= data_length) {
    printf("错误: 起始索引 %lu 超出数据长度 %lu\n", start_index, data_length);
    return;
  }

  /* 计算从起始索引开始可用的样本数量 */
  uint32_t available_samples = data_length - start_index;
  
  /* 确保只处理FFT_LENGTH长度的数据，避免超出支持范围 */
  uint32_t fft_samples = (available_samples < FFT_LENGTH) ? available_samples : FFT_LENGTH;

  
  if (processing_config.remove_dc) {
    /* 计算并移除直流分量 - 只对要处理的段计算DC值 */
    float dc_sum = 0.0f;
    for(uint32_t i = 0; i < fft_samples; i++) {
      dc_sum += adc_data[start_index + i];
    }
    float dc_component = dc_sum / fft_samples;
    
    for(uint32_t i = 0; i < fft_samples; i++) {
      fft_buffer[2*i] = (adc_data[start_index + i] - dc_component) * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION;
      fft_buffer[2*i+1] = 0.0f; // 虚部设为0
    }
  } else {
    /* 保留直流分量 */
    for(uint32_t i = 0; i < fft_samples; i++) {
      fft_buffer[2*i] = adc_data[start_index + i] * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION;
      fft_buffer[2*i+1] = 0.0f; // 虚部设为0
    }
  }
  
  /* 零填充剩余部分 */
  for(uint32_t i = fft_samples; i < FFT_LENGTH; i++) {
    fft_buffer[2*i] = 0.0f;
    fft_buffer[2*i+1] = 0.0f;
  }
}

/**
 * @brief 执行FFT并构建频谱数组 (合并原来的重复操作)
 * @param fft_buffer 输入FFT缓冲区
 * @param magnitude_buffer 输出幅度谱缓冲区
 * @retval None
 */
void ExecuteFFTAndBuildSpectrum(float* fft_buffer, float* magnitude_buffer)
{
  // uint32_t start_time = osKernelGetTickCount();
  
  /* 执行FFT初始化 */
  arm_status status = arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH, 0, 1);
  if (status != ARM_MATH_SUCCESS) {
    printf("错误: FFT初始化失败 (状态=%d)\n", status);
    return;
  }
  
  /* 执行FFT */
  arm_cfft_radix4_f32(&scfft, fft_buffer);
  
  /* 计算幅度谱 */
  uint32_t valid_bins = FFT_LENGTH / 2;
  for(uint32_t i = 0; i < valid_bins; i++) {
    float32_t real = fft_buffer[2 * i];
    float32_t imag = fft_buffer[2 * i + 1];
    magnitude_buffer[i] = sqrtf(real * real + imag * imag);
  }
  
  // uint32_t end_time = osKernelGetTickCount();
  // uint32_t tick_freq_ms = 1000 / osKernelGetTickFreq();
  // printf("FFT处理完成: 耗时约%lu毫秒\n", (end_time - start_time) * tick_freq_ms);
  
  /* 检查前10个频点是否都接近零 */
  // uint8_t all_zero = 1;
  // for(uint32_t i = 0; i < 10 && i < valid_bins; i++) {
  //   if (magnitude_buffer[i] > 0.001f) {  // 阈值0.001
  //     all_zero = 0;
  //     break;
  //   }
  // }
  // if (all_zero) {
  //   printf("警告: 前10个频点幅度都接近零! 检查FFT计算.\n");
  // }
}

/**
 * @brief 输出时域数据
 * @retval None
 */
void OutputTimeDomainData(void)
{
  #if USE_DUAL_ADC_INTERLEAVED
    /* 输出交错采样数据 */
    for(uint32_t i = 0; i < ADC_BUFFER_SIZE * 2; i++) {
      float voltage = (float)(merged_adc_data[i] * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION);
      uint32_t timestamp = time_domain_sample_index * TIME_DOMAIN_SAMPLE_INTERVAL_US;
      printf("%.6f,%lu\n", voltage, timestamp);
      time_domain_sample_index++;
    }
    
  #elif USE_DUAL_ADC_SIMULTANEOUS
    /* 输出同步采样数据 */
    for (uint32_t i = 0; i < ADC_BUFFER_SIZE; i++) {
      float voltage1 = ADC_ToVoltage(adc1_data_8bit[i]);
      float voltage2 = ADC_ToVoltage(adc2_data_8bit[i]);
      printf("ADC1/2: %.6f,%.6f\n", voltage1, voltage2);
    }
  #endif
}

/**
 * @brief 输出频谱数据
 * @retval None
 */
void OutputFrequencySpectrum(void)
{

  /* 输出有效频谱范围 (0 到采样率/2) */
  uint32_t valid_bins = FFT_LENGTH / 2;

  #if USE_DUAL_ADC_INTERLEAVED
    printf("=== ADC1/2 交替采样频谱数据 ===\n");
    
    
  #elif USE_DUAL_ADC_SIMULTANEOUS
    printf("=== ADC1/2 同步采样频谱数据 ===\n");
    
    for(uint32_t i = 0; i < valid_bins; i++) {
      float frequency = processing_config.shi_coefficient * (float)i *
                       processing_config.sampling_rate / FFT_LENGTH;
      printf("[%.2fHz]  ADC1/2:%.6f,%.6f\n", 
             frequency, adc1_magnitude_array[i], adc2_magnitude_array[i]);
    }
  #endif
}

/**
 * @brief 计算直流分量
 * @param data 数据缓冲区
 * @param length 数据长度
 * @retval float 直流分量值
 */
float CalculateDCComponent(uint16_t* data, uint32_t length)
{
  float dc_sum = 0.0f;
  for(uint32_t i = 0; i < length; i++) {
    dc_sum += data[i];
  }
  return dc_sum / length;
}

/* =============================================================================
 * ADC采样控制函数
 * ============================================================================= */

/**
 * @brief 启动ADC采样
 * 初始化DMA和定时器开始采样
 * @retval None
 */
void ADC_Processing_StartSampling(void)
{
  printf("尝试启动ADC采样，当前状态: adc_sampling_active = %d\n", adc_sampling_active);
  
  if (!adc_sampling_active) {
    /* 重置缓冲区计数 */
    buffer_fill_count = ADC_DEFAULT_BUFFER_COUNT;


    
    /* 启动DMA传输 */
    HAL_ADC_Start(&hadc2);
    HAL_StatusTypeDef dma_status = HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
    printf("DMA启动状态: %d\n", dma_status);
    
    /* 启动定时器，开始ADC触发 */
    HAL_StatusTypeDef tim_status = HAL_TIM_Base_Start(&htim3);
    printf("定时器启动状态: %d\n", tim_status);
    
    /* 更新状态标志 */
    adc_sampling_active = ADC_SAMPLING_ACTIVE_DEFAULT;
    
    printf("✓ ADC采样已启动，状态设置为: %d\n", adc_sampling_active);
  } else {
    printf("⚠ ADC已在运行中，跳过启动操作\n");
  }
}

/**
 * @brief 停止ADC采样
 * 当达到设定的缓冲区采样次数时自动调用，也可手动调用
 * @retval None
 */
void ADC_Processing_StopSampling(void)
{
  printf("尝试停止ADC采样，当前状态: adc_sampling_active = %d\n", adc_sampling_active);
  
  if (adc_sampling_active) {
    /* 停止DMA传输 */

    // HAL_StatusTypeDef adc1_status = HAL_ADC_Stop(&hadc1);
    // HAL_StatusTypeDef adc2_status = HAL_ADC_Stop(&hadc2);

    HAL_StatusTypeDef dma_status = HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    HAL_ADC_Stop_DMA(&hadc2);

    printf("DMA停止状态: %d\n", dma_status);
    
    /* 停止定时器，停止ADC触发 */
    HAL_StatusTypeDef tim_status = HAL_TIM_Base_Stop(&htim3);
    printf("定时器停止状态: %d\n", tim_status);
    
    /* 复位定时器计数器 */
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    
    /* 更新状态标志 */
    adc_sampling_active = ADC_SAMPLING_INACTIVE;
    
    printf("✓ ADC采样已停止，状态设置为: %d\n", adc_sampling_active);
  } else {
    printf("⚠ ADC已处于停止状态，跳过停止操作\n");
  }
}

/**
 * @brief 获取当前采样状态
 * @retval uint8_t 1:正在采样, 0:已停止采样
 */
uint8_t ADC_Processing_IsActive(void)
{
  return adc_sampling_active;
}

/**
 * @brief 设置自动停止采样功能
 * @param enabled 1:启用自动停止, 0:禁用自动停止
 * @retval None
 */
void ADC_Processing_SetAutoStopEnabled(uint8_t enabled)
{
  auto_stop_enabled = enabled ? ADC_AUTO_STOP_ENABLED : ADC_AUTO_STOP_DISABLED;
  printf("ADC auto-stop sampling %s\n", auto_stop_enabled ? "enabled" : "disabled");
}

/**
 * @brief 获取自动停止采样功能状态
 * @return 1:启用自动停止, 0:禁用自动停止
 */
uint8_t ADC_Processing_IsAutoStopEnabled(void)
{
  return auto_stop_enabled;
}

/**
 * @brief 获取当前已填充的缓冲区数量
 * @return 已填充的缓冲区数量
 */
uint32_t ADC_Processing_GetBufferFillCount(void)
{
  return buffer_fill_count;
}

/**
 * @brief 获取最大缓冲区填充数量
 * @return 最大缓冲区填充数量
 */
uint32_t ADC_Processing_GetMaxBufferFillCount(void)
{
  return max_buffer_fill_count;
}

/**
 * @brief 手动触发频域处理 - 支持任意段FFT计算
 * @param adc_data ADC数据缓冲区指针
 * @param start_index 开始计算的数据索引 (从adc_data[start_index]开始)
 * @param data_length 数据总长度，用于边界检查
 * @param fft_buffer FFT输入/输出缓冲区 (大小为2*FFT_LENGTH)
 * @param magnitude_buffer 幅度谱输出缓冲区 (大小为FFT_LENGTH/2)
 * @note 函数会从adc_data[start_index]开始提取FFT_LENGTH个样本进行FFT计算
 *       可用于将大量样本(如16384)分段(如4段，每段4096个点)处理
 * @retval None
 */
void ADC_Processing_TriggerFFT(uint16_t* adc_data, uint32_t start_index, uint32_t data_length, float* fft_buffer, float* magnitude_buffer)
{
    uint32_t sample_window = (start_index + FFT_LENGTH > data_length) ? (data_length - start_index) : FFT_LENGTH;
    
    printf("触发FFT处理: 起始索引=%lu, 总数据长度=%lu, 处理窗口大小=%lu\n", 
           start_index, data_length, sample_window);
    
    /* 验证输入参数 */
    if (adc_data == NULL || fft_buffer == NULL || magnitude_buffer == NULL) {
        printf("错误: FFT输入参数为空指针\n");
        return;
    }
    
    if (start_index >= data_length) {
        printf("错误: 起始索引(%lu)超出数据总长度(%lu)\n", start_index, data_length);
        return;
    }
    
    /* 准备FFT输入并执行FFT */
    PrepareFFTInput(adc_data, start_index, data_length, fft_buffer);
    ExecuteFFTAndBuildSpectrum(fft_buffer, magnitude_buffer);
}

/**
 * @brief 将ADC原始值转换为电压值
 * @param adc_value ADC原始值 (8位)
 * @retval float 电压值 (V)
 */
float ADC_ToVoltage(uint16_t adc_value)
{
  return (float)adc_value * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION;
}

/**
 * @brief 查找基波分量 - 重构使用配置参数
 * @param min_freq 搜索的最小频率 (Hz)
 * @param max_freq 搜索的最大频率 (Hz)
 * @retval fundamental_result_t 基波查找结果
 */
fundamental_result_t FindFundamentalComponent(float min_freq, float max_freq, float* magnitude_array)
{
  fundamental_result_t result = {0};
  result.found = 0;
  
  uint32_t valid_bins = FFT_LENGTH / 2;
  
  /* 使用配置参数计算bin索引范围 */
  uint32_t start_bin = (uint32_t)(min_freq * FFT_LENGTH / 
                      (processing_config.shi_coefficient * processing_config.sampling_rate));
  uint32_t end_bin = (uint32_t)(max_freq * FFT_LENGTH / 
                    (processing_config.shi_coefficient * processing_config.sampling_rate));
  
  /* 确保索引在有效范围内 */
  if (start_bin >= valid_bins) start_bin = valid_bins - 1;
  if (end_bin >= valid_bins) end_bin = valid_bins - 1;
  if (start_bin > end_bin) {
    printf("# 警告: 频率范围 %.2f - %.2f Hz 无效\n", min_freq, max_freq);
    return result;
  }

  /* 查找最大幅度值 */
  uint32_t search_length = end_bin - start_bin + 1;
  float max_magnitude;
  uint32_t max_index_relative;
  
  arm_max_f32(&magnitude_array[start_bin], search_length, &max_magnitude, &max_index_relative);
  
  /* 计算结果 */
  uint32_t actual_bin_index = start_bin + max_index_relative;
  float fundamental_frequency = processing_config.shi_coefficient * (float)actual_bin_index * 
                               processing_config.sampling_rate / FFT_LENGTH;
  
  /* 填充结果 */
  result.fundamental_frequency = fundamental_frequency;
  result.fundamental_magnitude = max_magnitude;
  result.fundamental_index = actual_bin_index;
  result.found = 1;
  
  printf("# 基波分量查找结果:\n");
  printf("# 频率: %.2f Hz, 幅度: %.6f, 索引: %d (搜索范围: %d-%d)\n", 
         result.fundamental_frequency, result.fundamental_magnitude, 
         result.fundamental_index, start_bin, end_bin);
  
  return result;
}


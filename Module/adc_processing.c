/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : adc_processing.c
  * @brief          : ADC采样与频域处理相关函数
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
static uint32_t last_fft_update_time = 0;
static uint32_t time_domain_sample_index = 0;

/* ADC采样控制变量 */
static uint32_t buffer_fill_count = 0;         // 已采集的缓冲区数量
static uint32_t max_buffer_fill_count = 5;     // 最大采集缓冲区数量，可配置
static uint8_t adc_sampling_active = 1;        // ADC采样状态标志
static uint8_t auto_stop_enabled = 1;          // 自动停止采样使能标志，默认开启

/* Private function prototypes -----------------------------------------------*/
/* 底层数据处理函数 */
static void ExtractDualADCData(uint16_t* dma_buffer);
static void PrepareFFTInput(uint16_t* adc_data, uint32_t data_length, float* fft_buffer);
static void ExecuteFFTAndBuildSpectrum(float* fft_buffer, float* magnitude_buffer);

/* 数据输出函数 */
static void OutputTimeDomainData(void);
static void OutputFrequencySpectrum(void);

/* 工具函数 */
static float CalculateDCComponent(uint16_t* data, uint32_t length);

/* Functions implementation --------------------------------------------------*/

/**
 * @brief ADC处理模块初始化
 * @param max_buffers 最大采样缓冲区数量，设置为0则使用默认值(5)
 * @param enable_auto_stop 是否启用自动停止采样，0:禁用, 1:启用
 * @retval None
 */
void ADC_Processing_Init(uint32_t max_buffers, uint8_t enable_auto_stop)
{
  /* 清空DMA缓冲区并确保缓存一致 */
  memset(dmabuffer_ping, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  memset(dmabuffer_pong, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  /* 清除DMA缓冲区的D-Cache，确保DMA能够正确写入 */
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_ping, ADC_BUFFER_SIZE * sizeof(uint16_t));
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_pong, ADC_BUFFER_SIZE * sizeof(uint16_t));

  /* 初始化时间控制变量 */
  last_fft_update_time = 0;
  time_domain_sample_index = 0;
  
  /* 初始化采样控制变量 */
  buffer_fill_count = 0;
  adc_sampling_active = 1;
  
  /* 设置最大缓冲区数量 */
  if (max_buffers > 0) {
    max_buffer_fill_count = max_buffers;
  } else {
    max_buffer_fill_count = 5; /* 默认值 */
  }
  
  /* 设置自动停止使能状态 */
  auto_stop_enabled = enable_auto_stop ? 1 : 0;

  #if USE_DUAL_ADC_INTERLEAVED || USE_DUAL_ADC_SIMULTANEOUS
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);

    /* 启动ADC2 */
    HAL_ADC_Start(&hadc2);
    /* 启动ADC双通道模式DMA传输 */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
  #endif
}

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
  OutputTimeDomainData();
  
  /* 步骤4: 频域处理 (按配置的时间间隔执行) */
  uint32_t current_time = HAL_GetTick();
  if (current_time - last_fft_update_time >= processing_config.fft_update_interval_ms) {
    last_fft_update_time = current_time;
    
    #if USE_DUAL_ADC_INTERLEAVED
      ADC_Processing_TriggerFFT(merged_adc_data, ADC_BUFFER_SIZE * 2, 
                               merged_fft_inputbuf, merged_magnitude_array);
    #elif USE_DUAL_ADC_SIMULTANEOUS
      /* 处理ADC1数据 */
      ADC_Processing_TriggerFFT((uint16_t*)adc1_data_8bit, ADC_BUFFER_SIZE, 
                               adc1_fft_inputbuf, adc1_magnitude_array);
      
      /* 处理ADC2数据 */
      ADC_Processing_TriggerFFT((uint16_t*)adc2_data_8bit, ADC_BUFFER_SIZE, 
                               adc2_fft_inputbuf, adc2_magnitude_array);
    #endif
    
    // OutputFrequencySpectrum();
  }
  
  /* 步骤5: 检查是否达到最大采样数量 */
  buffer_fill_count++;
  if (auto_stop_enabled && adc_sampling_active && buffer_fill_count >= max_buffer_fill_count) {
    ADC_Processing_StopSampling();
  }
}

/* =============================================================================
 * 私有函数实现 - 重构后的模块化功能
 * ============================================================================= */

/**
 * @brief 从DMA缓冲区提取双ADC数据
 * @param dma_buffer DMA缓冲区指针
 * @retval None
 */
static void ExtractDualADCData(uint16_t* dma_buffer)
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
 * @brief 准备FFT输入数据
 * @param adc_data ADC数据缓冲区
 * @param data_length 数据长度
 * @param fft_buffer 目标FFT缓冲区
 * @retval None
 */
static void PrepareFFTInput(uint16_t* adc_data, uint32_t data_length, float* fft_buffer)
{
  uint32_t fft_samples = (data_length < FFT_LENGTH) ? data_length : FFT_LENGTH;
  
  if (processing_config.remove_dc) {
    /* 计算并移除直流分量 */
    float dc_component = CalculateDCComponent(adc_data, data_length);
    
    for(uint32_t i = 0; i < fft_samples; i++) {
      fft_buffer[2*i] = (adc_data[i] - dc_component) * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION;
      fft_buffer[2*i+1] = 0.0f; // 虚部设为0
    }
  } else {
    /* 保留直流分量 */
    for(uint32_t i = 0; i < fft_samples; i++) {
      fft_buffer[2*i] = adc_data[i] * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION;
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
static void ExecuteFFTAndBuildSpectrum(float* fft_buffer, float* magnitude_buffer)
{
  /* 执行FFT */
  arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH, 0, 1);
  arm_cfft_radix4_f32(&scfft, fft_buffer);
  
  /* 同时计算幅度谱 - 避免重复循环 */
  uint32_t valid_bins = FFT_LENGTH / 2;
  for(uint32_t i = 0; i < valid_bins; i++) {
    float32_t real = fft_buffer[2 * i];
    float32_t imag = fft_buffer[2 * i + 1];
    magnitude_buffer[i] = sqrtf(real * real + imag * imag);
  }
}

/**
 * @brief 输出时域数据
 * @retval None
 */
static void OutputTimeDomainData(void)
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
static void OutputFrequencySpectrum(void)
{
  if (processing_config.remove_dc) {
    printf("=== 频谱数据 (滤除直流分量) ===\n");
  } else {
    printf("=== 频谱数据 (包含直流分量) ===\n");
  }

  /* 输出有效频谱范围 (0 到采样率/2) */
  uint32_t valid_bins = FFT_LENGTH / 2;

  #if USE_DUAL_ADC_INTERLEAVED
    printf("=== 交替采样频谱数据 ===\n");
    for(uint32_t i = 0; i < valid_bins; i++) {
      float frequency = processing_config.shi_coefficient * (float)i * 
                       processing_config.sampling_rate / FFT_LENGTH;
      printf("%.2f,%.6f\n", frequency, merged_magnitude_array[i]);
    }
  #elif USE_DUAL_ADC_SIMULTANEOUS
    printf("=== ADC1频谱数据 ===\n");
    for(uint32_t i = 0; i < valid_bins; i++) {
      float frequency = processing_config.shi_coefficient * (float)i * 
                       processing_config.sampling_rate / FFT_LENGTH;
      printf("%.2f,%.6f\n", frequency, adc1_magnitude_array[i]);
    }
    
    printf("=== ADC2频谱数据 ===\n");
    for(uint32_t i = 0; i < valid_bins; i++) {
      float frequency = processing_config.shi_coefficient * (float)i * 
                       processing_config.sampling_rate / FFT_LENGTH;
      printf("%.2f,%.6f\n", frequency, adc2_magnitude_array[i]);
    }
  #endif
  
  printf("=== 频谱数据结束 ===\n");
}

/**
 * @brief 计算直流分量
 * @param data 数据缓冲区
 * @param length 数据长度
 * @retval float 直流分量值
 */
static float CalculateDCComponent(uint16_t* data, uint32_t length)
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
 * @brief 停止ADC采样
 * 当达到设定的缓冲区采样次数时自动调用，也可手动调用
 * @retval None
 */
void ADC_Processing_StopSampling(void)
{
  if (adc_sampling_active) {
    /* 停止定时器，停止ADC触发 */
    HAL_TIM_Base_Stop(&htim3);
    
    /* 停止DMA传输 */
    HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    
    /* 复位定时器计数器 */
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    
    /* 更新状态标志 */
    adc_sampling_active = 0;
    
    printf("ADC sampling stopped after %lu buffers\n", buffer_fill_count);
  }
}

/**
 * @brief 恢复ADC采样
 * @retval None
 */
void ADC_Processing_ResumeSampling(void)
{
  if (!adc_sampling_active) {
    /* 重置缓冲区计数 */
    buffer_fill_count = 0;
    
    /* 重新启动DMA传输 */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
    
    /* 重新启动定时器，恢复ADC触发 */
    HAL_TIM_Base_Start(&htim3);
    
    /* 更新状态标志 */
    adc_sampling_active = 1;
    
    printf("ADC sampling resumed\n");
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
  auto_stop_enabled = enabled ? 1 : 0;
  printf("ADC auto-stop sampling %s\n", auto_stop_enabled ? "enabled" : "disabled");
}

/* =============================================================================
 * 保留的公共接口函数 (兼容性)
 * ============================================================================= */

/**
 * @brief 获取频谱数组指针 (新增接口)
 * @param adc_channel ADC通道编号 (1或2，仅在同步采样模式下使用)
 * @retval float* 幅度谱数组指针
 */
float* ADC_Processing_GetMagnitudeArray(uint8_t adc_channel)
{
  #if USE_DUAL_ADC_INTERLEAVED
    return merged_magnitude_array;
  #elif USE_DUAL_ADC_SIMULTANEOUS
    return (adc_channel == 1) ? adc1_magnitude_array : adc2_magnitude_array;
  #endif
}

/**
 * @brief 获取有效频谱bins数量 (新增接口)
 * @retval uint32_t 有效频谱bins数量
 */
uint32_t ADC_Processing_GetValidBins(void)
{
  return FFT_LENGTH / 2;
}

/**
 * @brief 手动触发频域处理 (新增接口)
 * @retval None
 */
void ADC_Processing_TriggerFFT(uint16_t* adc_data, uint32_t data_length, float* fft_buffer, float* magnitude_buffer)
{
    PrepareFFTInput(adc_data, data_length, fft_buffer);
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

/* =============================================================================
 * 兼容性接口 (保留旧接口以确保兼容性)
 * ============================================================================= */

/**
 * @brief 兼容性接口：按照VOFA协议输出时域波形数据
 * @param merged_data 合并后的交替采样数据缓冲区
 * @param sample_count 总样本数
 * @retval None
 */
void PrintTimeDomainDataVOFA(uint16_t* merged_data, uint32_t sample_count)
{
  for(uint32_t i = 0; i < sample_count; i++) {
    float voltage = (float)(merged_data[i] * ADC_REFERENCE_VOLTAGE / ADC_8BIT_RESOLUTION);
    uint32_t timestamp = time_domain_sample_index * TIME_DOMAIN_SAMPLE_INTERVAL_US;
    printf("%.6f,%lu\n", voltage, timestamp);
    time_domain_sample_index++;
  }
}

/**
 * @brief 兼容性接口：按照VOFA协议输出频谱数据
 * @param actual_sampling_rate 实际采样率 (Hz) - 将被忽略，使用配置值
 * @param remove_dc 是否已滤除直流分量 - 将被忽略，使用配置值
 * @param shi 神秘系数 - 将被忽略，使用配置值
 * @retval None
 */
void PrintFrequencySpectrumVOFA(float actual_sampling_rate, uint8_t remove_dc, float shi)
{
  OutputFrequencySpectrum(); // 直接调用新的内部函数
}

/**
 * @brief 兼容性接口：构建幅度谱数组
 * @param actual_sampling_rate 实际采样率 (Hz) - 将被忽略
 * @param remove_dc 是否已滤除直流分量 - 将被忽略
 * @param shi 神秘系数 - 将被忽略
 * @retval None
 */
void BuildMagnitudeArray(float actual_sampling_rate, uint8_t remove_dc, float shi)
{
  /* 此函数功能已整合到ExecuteFFTAndBuildSpectrum中 */
  /* 为兼容性保留，但实际不执行任何操作 */
}

/**
 * @brief 兼容性接口：频域处理函数
 * @param adc_data ADC数据缓冲区指针
 * @param data_length ADC数据长度
 * @param update_interval_ms 更新间隔时间（毫秒） - 将被忽略，使用配置值
 * @retval None
 */
void ProcessFrequencyDomain(uint16_t* adc_data, uint32_t data_length, uint32_t update_interval_ms)
{
  uint32_t current_time = HAL_GetTick();
  if (current_time - last_fft_update_time >= processing_config.fft_update_interval_ms) {
    last_fft_update_time = current_time;
    
    #if USE_DUAL_ADC_INTERLEAVED
      ADC_Processing_TriggerFFT(adc_data, data_length, 
                               merged_fft_inputbuf, merged_magnitude_array);
    #elif USE_DUAL_ADC_SIMULTANEOUS
      /* 使用ADC1数据处理 */
      ADC_Processing_TriggerFFT(adc_data, data_length, 
                               adc1_fft_inputbuf, adc1_magnitude_array);
    #endif
  }
}

/**
 * @brief 兼容性接口：无参数初始化函数
 * 使用默认参数调用新的初始化函数，保持向后兼容性
 * @retval None
 */
void ADC_Processing_Init_Compat(void)
{
  /* 使用默认参数调用新版初始化函数:
   * - 默认最大缓冲区数量: 5
   * - 默认启用自动停止: 1
   */
  ADC_Processing_Init(5, 1);
}

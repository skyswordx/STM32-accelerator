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

/* Private variables ---------------------------------------------------------*/
#if USE_DUAL_ADC_INTERLEAVED 
/* 合并的ADC数据 - 优化：直接从DMA缓冲区填充，无需中间数组 */
uint16_t merged_adc_data[ADC_BUFFER_SIZE * 2]; // Buffer for merged interleaved ADC data
#endif

#if USE_DUAL_ADC_SIMULTANEOUS
/* 双ADC同步采样模式 - 优化：直接使用DMA缓冲区 */
uint8_t adc1_data_8bit[ADC_BUFFER_SIZE]; // Buffer for ADC1 data
uint8_t adc2_data_8bit[ADC_BUFFER_SIZE]; // Buffer for ADC2 data
#endif

volatile uint8_t adc_conversion_complete = 0; // Flag to indicate conversion complete
volatile uint8_t buffer_swap_flag = 0; // Flag to indicate buffer swap is needed

/* 双缓冲机制 - 使用链接器自动分配内存避免地址冲突 */
uint16_t dmabuffer_ping[ADC_BUFFER_SIZE] __attribute__((aligned(32))); // Ping buffer for DMA transfers - 32字节对齐
uint16_t dmabuffer_pong[ADC_BUFFER_SIZE] __attribute__((aligned(32))); // Pong buffer for DMA transfers - 32字节对齐

uint16_t* active_dma_buffer = dmabuffer_ping;     // 当前DMA写入的缓冲区
uint16_t* processing_buffer = dmabuffer_pong;     // 当前处理的缓冲区

/* FFT相关变量定义 */
arm_cfft_radix4_instance_f32 scfft;//定义scfft结构
float FFT_InputBuf[FFT_LENGTH*2];  //FFT输入数组（复数形式：实部+虚部）
float magnitude_array[FFT_LENGTH/2];  //幅度谱数组（只保留有效频谱范围）

/* Private function prototypes -----------------------------------------------*/

/* Functions implementation --------------------------------------------------*/

/**
 * @brief ADC处理模块初始化
 * @retval None
 */
void ADC_Processing_Init(void)
{
  /* 清空DMA缓冲区并确保缓存一致 */
  memset(dmabuffer_ping, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  memset(dmabuffer_pong, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  /* 清除DMA缓冲区的D-Cache，确保DMA能够正确写入 */
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_ping, ADC_BUFFER_SIZE * sizeof(uint16_t));
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_pong, ADC_BUFFER_SIZE * sizeof(uint16_t));

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
 * @brief 处理完整的缓冲区数据
 * @param buffer 要处理的缓冲区指针
 * @retval None
 */
void ProcessCompleteBuffer(uint16_t* buffer)
{
  /* 在主任务中进行缓存操作，避免在中断上下文中的时序问题 */
  /* 刷新DMA缓冲区的D-Cache */
  SCB_InvalidateDCache_by_Addr((uint32_t*)buffer, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  #if USE_DUAL_ADC_INTERLEAVED 
     /* 直接从DMA缓冲区解包数据到merged_adc_data数组，避免中间数组：
      * buffer[j] = 0xXXYY (16位)
      * 其中 XX (高8位) = ADC2 (slave)
      * 其中 YY (低8位) = ADC1 (master)
      * 交替排列为：merged_data[0] = ADC1[0], merged_data[1] = ADC2[0]
      * merged_data[2] = ADC1[1], merged_data[3] = ADC2[1]
      * ...
      * 这样可以实现双通道的有效采样
      */
      for(uint32_t j = 0; j < ADC_BUFFER_SIZE; j++)
      {
        merged_adc_data[j * 2] = (uint16_t)(buffer[j] & 0xFF);        // 偶数索引放ADC1数据 (低8位)
        merged_adc_data[j * 2 + 1] = (uint16_t)((buffer[j] >> 8) & 0xFF); // 奇数索引放ADC2数据 (高8位)
      }
      
  #elif USE_DUAL_ADC_SIMULTANEOUS
      for (uint32_t j = 0; j < ADC_BUFFER_SIZE; j++)
      {
        adc1_data_8bit[j] = (uint8_t)(buffer[j] & 0xFF);
        adc2_data_8bit[j] = (uint8_t)((buffer[j] >> 8) & 0xFF);

        printf("ADC1/2: %.6f,%.6f\n", 
               ADC_ToVoltage(adc1_data_8bit[j]), 
               ADC_ToVoltage(adc2_data_8bit[j]));
      }
  #endif
}

/**
 * @brief 按照VOFA协议输出时域波形数据
 * @param merged_data 合并后的交替采样数据缓冲区
 * @param sample_count 总样本数 (每个样本包含电压和时间戳)
 * @retval None
 */
void PrintTimeDomainDataVOFA(uint16_t* merged_data, uint32_t sample_count)
{
  static uint32_t sample_index = 0; // 静态变量保存样本索引

  /* 输出交替采样数据，每个样本包含电压和时间戳 */
  for(uint32_t i = 0; i < sample_count; i++)
  {
    float voltage =  (float)(merged_data[i] * 3.3f / ADC_8BIT_RESOLUTION);
    
    /* 计算每个样本的时间戳（微秒）
     * 使用样本索引来计算时间戳，假设每个样本间隔固定
     * 这里假设交替采样的有效采样率为20kHz（每个样本间隔50微秒）
     */
    uint32_t timestamp = sample_index * 50; // 每个样本间隔50微秒
    
    /* VOFA协议格式：voltage,timestamp */
    printf("%.6f,%lu\n", voltage, timestamp);
    
    sample_index++;
  }
}

/**
 * @brief 将ADC原始值转换为电压值
 * @param adc_value ADC原始值 (8位)
 * @retval float 电压值 (V)
 */
float ADC_ToVoltage(uint16_t adc_value)
{
  /* 8位ADC，参考电压3.3V */
  return (float)adc_value * 3.3f / ADC_8BIT_RESOLUTION;
}

/**
 * @brief 按照VOFA协议输出频谱数据
 * @param actual_sampling_rate 实际采样率 (Hz)
 * @param remove_dc 是否已滤除直流分量
 * @param shi 神秘系数
 * @retval None
 */
void PrintFrequencySpectrumVOFA(float actual_sampling_rate, uint8_t remove_dc, float shi)
{
  if (remove_dc == 1) {
    printf("=== 频谱数据 (滤除直流分量) ===\n");
  } else {
    printf("=== 频谱数据 (包含直流分量) ===\n");
  }

  /* 计算幅度谱并输出 */
  // 全频谱，实际上有效频率范围只有 0 到采样率的 1/2（奈奎斯特频率）
  // 采样率的 1/2 到采样率本身的部分是镜像
  for(uint32_t i = 0; i < FFT_LENGTH; i++) {
    float32_t real = FFT_InputBuf[2 * i];
    float32_t imag = FFT_InputBuf[2 * i + 1];
    float32_t magnitude = sqrtf(real * real + imag * imag);
    
    /* 神秘公式 */
    /* 输出频率和对应的幅度值 */
    float frequency = shi * (float)i * actual_sampling_rate / FFT_LENGTH;
    printf("%.2f,%.6f\n", frequency, magnitude);
  }
  
  printf("=== 频谱数据结束 ===\n");
}

/**
 * @brief 构建幅度谱数组
 * @param actual_sampling_rate 实际采样率 (Hz)
 * @param remove_dc 是否已滤除直流分量
 * @param shi 神秘系数
 * @retval None
 */
void BuildMagnitudeArray(float actual_sampling_rate, uint8_t remove_dc, float shi)
{
  /* 只计算有效频谱范围 (0 到 采样率/2) */
  uint32_t valid_bins = FFT_LENGTH / 2;
  
  for(uint32_t i = 0; i < valid_bins; i++) {
    float32_t real = FFT_InputBuf[2 * i];
    float32_t imag = FFT_InputBuf[2 * i + 1];
    float32_t magnitude = sqrtf(real * real + imag * imag);

    /* 存储到幅度数组 */
    magnitude_array[i] = magnitude;
  }
}

/**
 * @brief 查找基波分量
 * @param min_freq 搜索的最小频率 (Hz)
 * @param max_freq 搜索的最大频率 (Hz)
 * @retval fundamental_result_t 基波查找结果
 */
fundamental_result_t FindFundamentalComponent(float min_freq, float max_freq)
{
  fundamental_result_t result = {0};
  result.found = 0;
  
  /* 系统参数 */
  static float actual_sampling_rate = 10730000.0f; // 10.73 MHz
  static float shi = 0.09f; // 神秘系数
  
  /* 有效频谱范围 */
  uint32_t valid_bins = FFT_LENGTH / 2;
  
  /* 直接通过频率计算bin索引范围 - 避免循环查找 */
  uint32_t start_bin = (uint32_t)(min_freq * FFT_LENGTH / (shi * actual_sampling_rate));
  uint32_t end_bin = (uint32_t)(max_freq * FFT_LENGTH / (shi * actual_sampling_rate));
  
  /* 确保索引在有效范围内 */
  if (start_bin >= valid_bins) start_bin = valid_bins - 1;
  if (end_bin >= valid_bins) end_bin = valid_bins - 1;
  if (start_bin > end_bin) {
    printf("# 警告: 频率范围 %.2f - %.2f Hz 无效\n", min_freq, max_freq);
    return result;
  }

  /* 计算搜索范围的长度 */
  uint32_t search_length = end_bin - start_bin + 1;

  /* 使用ARM DSP库的arm_max_f32函数查找最大幅度值 */
  float max_magnitude;
  uint32_t max_index_relative;
  
  arm_max_f32(&magnitude_array[start_bin], search_length, &max_magnitude, &max_index_relative);
  
  /* 计算实际的bin索引 */
  uint32_t actual_bin_index = start_bin + max_index_relative;
  
  /* 计算基波频率 */
  float fundamental_frequency = shi * (float)actual_bin_index * actual_sampling_rate / FFT_LENGTH;
  
  /* 填充结果 */
  result.fundamental_frequency = fundamental_frequency;
  result.fundamental_magnitude = max_magnitude;
  result.fundamental_index = actual_bin_index;
  result.found = 1;
  
  printf("# 基波分量查找结果:\n");
  printf("# 频率: %.2f Hz, 幅度: %.6f, 索引: %d (搜索范围: %d-%d)\n", 
         result.fundamental_frequency, 
         result.fundamental_magnitude, 
         result.fundamental_index,
         start_bin, end_bin);
  
  return result;
}

/**
 * @brief 频域处理函数 - 对ADC数据进行FFT分析并更新全局频谱数组
 * @param adc_data ADC数据缓冲区指针
 * @param data_length ADC数据长度
 * @param update_interval_ms 更新间隔时间（毫秒）
 * @retval None
 */
void ProcessFrequencyDomain(uint16_t* adc_data, uint32_t data_length, uint32_t update_interval_ms)
{
  static uint32_t last_update_time = 0;
  uint32_t current_time = HAL_GetTick();
  
  /* 检查是否到了更新时间 */
  if (current_time - last_update_time < update_interval_ms) {
    return; // 还没到更新时间，直接返回
  }
  
  /* 更新时间戳 */
  last_update_time = current_time;
  
  /* 系统参数 */
  static float actual_sampling_rate = 10730000.0f; // 10.73 MHz
  static float shi = 0.09f; // 神秘系数
  uint8_t remove_dc = 1; // 滤除直流分量
  
  /* 直接在全局FFT_InputBuf中准备FFT数据 */
  if (remove_dc == 1) {
    /* 计算直流分量 */
    float dc_component = 0.0f;
    for(uint32_t i = 0; i < data_length; i++) {
      dc_component += adc_data[i];
    }
    dc_component /= data_length;
    
    /* 填充FFT输入缓冲区并滤除直流分量 */
    uint32_t fft_samples = (data_length < FFT_LENGTH) ? data_length : FFT_LENGTH;
    for(uint32_t i = 0; i < fft_samples; i++) {
      FFT_InputBuf[2*i] = (adc_data[i] - dc_component) * 3.3f / ADC_8BIT_RESOLUTION; // 实部
      FFT_InputBuf[2*i+1] = 0.0f; // 虚部
    }
    
    /* 如果数据长度小于FFT长度，用零填充剩余部分 */
    for(uint32_t i = fft_samples; i < FFT_LENGTH; i++) {
      FFT_InputBuf[2*i] = 0.0f;
      FFT_InputBuf[2*i+1] = 0.0f;
    }
  } else {
    /* 填充FFT输入缓冲区，保留直流分量 */
    uint32_t fft_samples = (data_length < FFT_LENGTH) ? data_length : FFT_LENGTH;
    for(uint32_t i = 0; i < fft_samples; i++) {
      FFT_InputBuf[2*i] = adc_data[i] * 3.3f / ADC_8BIT_RESOLUTION; // 实部
      FFT_InputBuf[2*i+1] = 0.0f; // 虚部
    }
    
    /* 如果数据长度小于FFT长度，用零填充剩余部分 */
    for(uint32_t i = fft_samples; i < FFT_LENGTH; i++) {
      FFT_InputBuf[2*i] = 0.0f;
      FFT_InputBuf[2*i+1] = 0.0f;
    }
  }
  
  /* 初始化并执行FFT */
  arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH, 0, 1);
  arm_cfft_radix4_f32(&scfft, FFT_InputBuf);
  
  /* 计算幅度谱并更新全局数组 */
  BuildMagnitudeArray(actual_sampling_rate, remove_dc, shi);
}

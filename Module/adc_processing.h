/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : adc_processing.h
  * @brief          : Header for adc_processing.c file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_PROCESSING_H
#define __ADC_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"

/* External references -------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern UART_HandleTypeDef huart1;

/* Exported defines ----------------------------------------------------------*/
#define USE_DUAL_ADC_INTERLEAVED 0 // 使用双ADC交错模式
#define USE_DUAL_ADC_SIMULTANEOUS 1 // 使用双ADC同步采样模式

#define ADC_BUFFER_SIZE 1024 * 16 
#define ADC_8BIT_RESOLUTION 256.0f // 8位ADC分辨率

#define FFT_LENGTH ADC_BUFFER_SIZE //FFT长度

/* Exported types ------------------------------------------------------------*/
/* 频谱数据结构 */
typedef struct {
  float frequency;    // 频率
  float magnitude;    // 幅度
  uint32_t bin_index; // FFT bin 索引
} spectrum_data_t;

/* 基波分量结果结构 */
typedef struct {
  float fundamental_frequency;  // 基波频率 (Hz)
  float fundamental_magnitude;  // 基波幅度
  uint32_t fundamental_index;   // 基波在频谱数组中的索引
  uint8_t found;               // 是否找到基波 (1=找到, 0=未找到)
} fundamental_result_t;

/* Exported variables --------------------------------------------------------*/
#if USE_DUAL_ADC_INTERLEAVED 
/* 合并的ADC数据 - 优化：直接从DMA缓冲区填充，无需中间数组 */
extern uint16_t merged_adc_data[ADC_BUFFER_SIZE * 2]; // Buffer for merged interleaved ADC data
#endif

#if USE_DUAL_ADC_SIMULTANEOUS
/* 双ADC同步采样模式 - 优化：直接使用DMA缓冲区 */
extern uint8_t adc1_data_8bit[ADC_BUFFER_SIZE]; // Buffer for ADC1 data
extern uint8_t adc2_data_8bit[ADC_BUFFER_SIZE]; // Buffer for ADC2 data
#endif

extern volatile uint8_t adc_conversion_complete; // Flag to indicate conversion complete
extern volatile uint8_t buffer_swap_flag; // Flag to indicate buffer swap is needed

/* 双缓冲机制 - 使用链接器自动分配内存避免地址冲突 */
extern uint16_t dmabuffer_ping[ADC_BUFFER_SIZE]; // Ping buffer for DMA transfers - 32字节对齐
extern uint16_t dmabuffer_pong[ADC_BUFFER_SIZE]; // Pong buffer for DMA transfers - 32字节对齐

extern uint16_t* active_dma_buffer;     // 当前DMA写入的缓冲区
extern uint16_t* processing_buffer;     // 当前处理的缓冲区

/* FFT相关变量定义 */
extern arm_cfft_radix4_instance_f32 scfft;//定义scfft结构
extern float FFT_InputBuf[FFT_LENGTH*2];  //FFT输入数组（复数形式：实部+虚部）
extern float magnitude_array[FFT_LENGTH/2];  //幅度谱数组（只保留有效频谱范围）

/* Exported functions prototypes ---------------------------------------------*/
void SwapDMABuffers(void);
void ProcessCompleteBuffer(uint16_t* buffer);
void PrintTimeDomainDataVOFA(uint16_t* merged_data, uint32_t sample_count);
void PrintFrequencySpectrumVOFA(float actual_sampling_rate, uint8_t remove_dc, float shi);
float ADC_ToVoltage(uint16_t adc_value);
void BuildMagnitudeArray(float actual_sampling_rate, uint8_t remove_dc, float shi);
fundamental_result_t FindFundamentalComponent(float min_freq, float max_freq);
void ProcessFrequencyDomain(uint16_t* adc_data, uint32_t data_length, uint32_t update_interval_ms);

/* ADC采样处理相关初始化函数 */
void ADC_Processing_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_PROCESSING_H */

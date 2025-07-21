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

#define FFT_LENGTH  ADC_BUFFER_SIZE //FFT长度

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
extern arm_cfft_radix4_instance_f32 scfft;  //定义scfft结构

/* 交替采样模式的FFT数据结构 */
#if USE_DUAL_ADC_INTERLEAVED
extern float merged_fft_inputbuf[2 * FFT_LENGTH];    // 交替采样FFT输入数组（复数形式）
extern float merged_magnitude_array[FFT_LENGTH/2];    // 交替采样幅度谱数组

/* 同步采样模式的FFT数据结构 */
#elif USE_DUAL_ADC_SIMULTANEOUS
extern float adc1_fft_inputbuf[2 * FFT_LENGTH];      // ADC1 FFT输入数组（复数形式）
extern float adc2_fft_inputbuf[2 * FFT_LENGTH];      // ADC2 FFT输入数组（复数形式）
extern float adc1_magnitude_array[FFT_LENGTH/2];      // ADC1幅度谱数组
extern float adc2_magnitude_array[FFT_LENGTH/2];      // ADC2幅度谱数组
#endif

/* Exported functions prototypes ---------------------------------------------*/
/* 核心接口 - 重构后的清晰接口 */
void ADC_Processing_Init(uint32_t max_buffers, uint8_t enable_auto_stop);
void ADC_Processing_SetConfig(float sampling_rate, float shi_coefficient, 
                              uint8_t remove_dc, uint32_t fft_update_interval_ms);
void ADC_Processing_SetMaxBufferCount(uint32_t count);

/* ADC采样控制接口 */
void ADC_Processing_StopSampling(void);
void ADC_Processing_ResumeSampling(void);
uint8_t ADC_Processing_IsActive(void);
void ADC_Processing_SetAutoStopEnabled(uint8_t enabled);
uint8_t ADC_Processing_IsAutoStopEnabled(void);
uint32_t ADC_Processing_GetBufferFillCount(void);
uint32_t ADC_Processing_GetMaxBufferFillCount(void);

/* 数据处理接口 */
void SwapDMABuffers(void);
void ProcessCompleteBuffer(uint16_t* buffer);

/* 频域分析接口 */
void ADC_Processing_TriggerFFT(uint16_t* adc_data, uint32_t data_length, float* fft_buffer, float* magnitude_buffer);

fundamental_result_t FindFundamentalComponent(float min_freq, float max_freq, float* magnitude_array);

/* 内部处理函数声明 */
static void ExtractDualADCData(uint16_t* dma_buffer);
static void PrepareFFTInput(uint16_t* adc_data, uint32_t data_length, float* fft_buffer);
static void ExecuteFFTAndBuildSpectrum(float* fft_buffer, float* magnitude_buffer);
static void OutputFrequencySpectrum(void);

/* 工具函数 */
float ADC_ToVoltage(uint16_t adc_value);


#ifdef __cplusplus
}
#endif

#endif /* __ADC_PROCESSING_H */

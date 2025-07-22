#include "adc_processing_task.h"
#include "adc_processing.h"
#include "cmsis_os.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* 所有外部引用变量都已在 adc_processing.h 中声明，不需要在此重复声明 */



void StartADCProcessingTask(void *argument)
{
  /* 直接访问和初始化外部变量，精确控制 */
  /* 清空DMA缓冲区并确保缓存一致 */
  memset(dmabuffer_ping, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  memset(dmabuffer_pong, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  /* 清除DMA缓冲区的D-Cache，确保DMA能够正确写入 */
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_ping, ADC_BUFFER_SIZE * sizeof(uint16_t));
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_pong, ADC_BUFFER_SIZE * sizeof(uint16_t));

  /* 初始化时间控制变量 */
  last_fft_update_time = 0;
  time_domain_sample_index = 0;
  
  /* 初始化采样控制变量 - 简化设置只采集一个缓冲区 */
  buffer_fill_count = 0;           // 从0开始计数
  adc_sampling_active = 1;         // 开始时激活采样
  max_buffer_fill_count = 1;       // 只采集一个缓冲区就停止
  auto_stop_enabled = 0;           // 启用自动停止
  
  #if USE_DUAL_ADC_INTERLEAVED || USE_DUAL_ADC_SIMULTANEOUS
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);

    /* 启动定时器3作为时间戳基准和ADC触发源 */
    HAL_TIM_Base_Start(&htim3);
    
    /* 给一个短暂延迟确保定时器稳定运行 */
    osDelay(5);
    
    /* 启动ADC2 */
    HAL_ADC_Start(&hadc2);
    
    /* 给一个短暂延迟确保ADC2稳定运行 */
    osDelay(5);
    
    /* 启动ADC双通道模式DMA传输 - 简化调用 */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
  #endif

  /* 等待系统稳定 */
  osDelay(100);

  for(;;)
  {
    if(adc_conversion_complete)
    {
      adc_conversion_complete = 0;
      
      /* 交换缓冲区指针 */
      if(buffer_swap_flag)
      {
        buffer_swap_flag = 0; 
        SwapDMABuffers();
      }
      
      
      /* 检查是否需要处理数据（只在ADC仍在采样时处理） */
      if (adc_sampling_active) 
      {
        ProcessCompleteBuffer(processing_buffer);
      }
      
      /* 检查是否达到最大采样数量并停止ADC采样 */
      if (auto_stop_enabled && adc_sampling_active && buffer_fill_count >= max_buffer_fill_count) {
        printf("已达到最大采样缓冲区数量，停止ADC采样\n");
        ADC_Processing_StopSampling();
      }
    }
    osDelay(1);
  }
}

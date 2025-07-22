#include "adc_processing_task.h"
#include "adc_processing.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "stm32h7xx_hal.h" // 确保包含STM32 HAL库

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
  
  /* 初始化采样控制变量 + 设置最大缓冲区数量 + 设置自动停止使能状态*/
  buffer_fill_count = ADC_DEFAULT_BUFFER_COUNT;
  adc_sampling_active = ADC_SAMPLING_ACTIVE_DEFAULT;
  max_buffer_fill_count = ADC_DEFAULT_MAX_BUFFER_COUNT; // 设置需要的缓冲区数量
  auto_stop_enabled = ADC_AUTO_STOP_ENABLED; // 启用自动停止采样
  
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
      if(buffer_swap_flag){buffer_swap_flag = 0; SwapDMABuffers();}
      
      
      ProcessCompleteBuffer(processing_buffer);
      
      /* 检查是否达到最大采样数量 - 直接访问变量实现更精细控制 */
      if (auto_stop_enabled && adc_sampling_active && buffer_fill_count >= max_buffer_fill_count) 
      {
          /* 停止ADC采样 */
          ADC_Processing_StopSampling();
      }
    }
    osDelay(1);
  }
}

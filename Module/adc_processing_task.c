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
  auto_stop_enabled = 1;           // 启用自动停止
  
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
      if(buffer_swap_flag){
        buffer_swap_flag = 0; 
        SwapDMABuffers();
      }
      
      // ProcessCompleteBuffer(processing_buffer);
      /* 检查是否需要处理数据（只在ADC仍在采样时处理） */
      if (adc_sampling_active) {
       
        /* ===== 分解的 ProcessCompleteBuffer 功能 ===== */
        
        /* 步骤1: 缓存一致性处理 */
        SCB_InvalidateDCache_by_Addr((uint32_t*)processing_buffer, ADC_BUFFER_SIZE * sizeof(uint16_t));
        
        /* 步骤2: 数据提取和转换 - 从DMA缓冲区提取到adc1_data_8bit和adc2_data_8bit */
        ExtractDualADCData(processing_buffer);
        
        /* 步骤5: 更新缓冲区计数 */
        buffer_fill_count++;
        
        printf("已处理缓冲区 #%lu，数据已提取到 adc1_data_8bit 和 adc2_data_8bit\n", (unsigned long)buffer_fill_count);
      }
      
      /* 检查是否达到最大采样数量并停止ADC采样 */
      if (auto_stop_enabled && adc_sampling_active && buffer_fill_count >= max_buffer_fill_count) 
      {
          printf("===== 达到目标缓冲区数量，停止ADC采样 =====\n");
          
          /* 停止ADC采样 */
          ADC_Processing_StopSampling();
          
          printf("✓ ADC/DMA/TIM 已停止\n");
          printf("✓ adc1_data_8bit[%d] 数据已冻结\n", ADC_BUFFER_SIZE);
          printf("✓ adc2_data_8bit[%d] 数据已冻结\n", ADC_BUFFER_SIZE);
          printf("开始处理冻结的数据...\n\n");
          
          /* ===== 在ADC停止后执行的处理操作 ===== */
          
          // #if USE_DUAL_ADC_SIMULTANEOUS
          // {
          //   int i;
          //   for(i = 0; i < ADC_BUFFER_SIZE; i++) {
          //     float voltage1 = ADC_ToVoltage(adc1_data_8bit[i]);
          //     float voltage2 = ADC_ToVoltage(adc2_data_8bit[i]);
          //     printf("ADC1/2(Time): %.4f, %.4f\n", voltage1, voltage2);
          //   }
          // }
          // #endif
          
          /* 步骤4: 频域处理 */
          /* 执行FFT处理 */
          #if USE_DUAL_ADC_SIMULTANEOUS
          /* 处理ADC1数据 - 起始索引设为0，从头开始处理 */
          ADC_Processing_TriggerFFT((uint16_t*)adc1_data_8bit, 0, ADC_BUFFER_SIZE, 
                                  adc1_fft_inputbuf, adc1_magnitude_array);
          
          /* 处理ADC2数据 - 起始索引设为0，从头开始处理 */
          ADC_Processing_TriggerFFT((uint16_t*)adc2_data_8bit, 0, ADC_BUFFER_SIZE, 
                                  adc2_fft_inputbuf, adc2_magnitude_array);

          #endif
          for (int i = 0; i < FFT_LENGTH; i++) {
            // 有效的频率范围是0到采样率/2，这一部分存在了 adcx_magnitude_array
            // 和时域数据以及幅度一起输出
            float voltage1 = ADC_ToVoltage(adc1_data_8bit[i]);
            float voltage2 = ADC_ToVoltage(adc2_data_8bit[i]);
            printf("ADC1(Time/Magni): %.4f, %.4f\n", voltage1, adc1_magnitude_array[i]);


            if (i > FFT_LENGTH / 2) {
              // 按照对称性进行输出 
              printf("ADC1(Time/Magni): %.4f, %.4f\n", voltage1, adc1_magnitude_array[FFT_LENGTH - i]);
            }
          }


          printf("数据处理流程结束\n\n");
          
          /* 可选：重新开始采样或进入等待状态 */
          printf("等待2秒后重新开始采样...\n");
          osDelay(2000);
          
          /* 重置状态，准备下一次采样 */
          buffer_fill_count = 0;
          /* 注意：不要手动设置adc_sampling_active，让StartSampling函数内部处理 */
          ADC_Processing_StartSampling();
      }
    }
    osDelay(1);
  }
}

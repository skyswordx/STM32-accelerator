#include "adc_processing_task.h"
#include "adc_processing.h"


void StartADCProcessingTask(void *argument)
{
  /* Infinite loop */

  for(;;)
  {
    if(adc_conversion_complete)
    {
      adc_conversion_complete = 0;

      if(buffer_swap_flag)
      {
        buffer_swap_flag = 0;
        SwapDMABuffers();
      }
      
      ProcessCompleteBuffer(processing_buffer);
      
      /* 检查是否达到最大采样数量 - 现在在任务中实现而不是在处理函数中 */
      if (ADC_Processing_IsAutoStopEnabled() && 
          ADC_Processing_IsActive() && 
          ADC_Processing_GetBufferFillCount() >= ADC_Processing_GetMaxBufferFillCount()) {
        
          ADC_Processing_StopSampling();
          /* 可以在这里执行额外的清理或通知操作 */

          printf("ADC自动停止采样由任务执行，已处理 %lu 个缓冲区\r\n", 
                ADC_Processing_GetBufferFillCount());
      }
    }
    osDelay(1);
  }
}

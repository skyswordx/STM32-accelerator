#include "adc_task.h"
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

    }
    osDelay(1);
  }
}

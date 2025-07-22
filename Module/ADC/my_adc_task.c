#include "my_adc_task.h"


void StartADCProcessingTask2(void *argument) {
    
    /* 初始化内存空间并且启动定时器和双 ADC */


    for (;;) {
        // 处理ADC数据
        if (ADC_Processing_IsActive()) {
            uint16_t* current_buffer = active_dma_buffer;
            ProcessCompleteBuffer(current_buffer);
            SwapDMABuffers();
        }
        
        osDelay(1); // 延时1毫秒
    }
}
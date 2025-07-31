#include "my_dac_task.h"
#include "my_dac_config.h"
#include "my_parameter_config.h"  // 包含参数配置头文件

#include "my_dds.h" 

// 声明外部变量和函数
extern TIM_HandleTypeDef htim4;
extern DAC_HandleTypeDef hdac1;
extern uint16_t* g_dac_waveform_buffer;
extern uint16_t g_dac_sine[256];
extern uint16_t g_dac_square[256];
extern uint16_t g_dac_triangle[256];
extern uint16_t g_dac_cosine[256];

void StartDACProcessingTask(void *argument) {

    // // 启动定时器
    // HAL_TIM_Base_Start(&htim4);
    
    // // 启动DAC DMA传输
    // HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_square_64, 50, DAC_ALIGN_12B_R);




    
    for(;;)
    {
        
        osDelay(100); // 延时100毫秒
    }
}


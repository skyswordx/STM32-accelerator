#include "my_dac_task.h"
#include "my_dac_config.h"
#include "my_parameter_config.h"  // 包含参数配置头文件

// 声明外部变量和函数
extern TIM_HandleTypeDef htim4;
extern DAC_HandleTypeDef hdac1;
extern uint16_t* g_dac_waveform_buffer;
extern uint16_t g_dac_sine[256];
extern uint16_t g_dac_square[256];
extern uint16_t g_dac_triangle[256];
extern uint16_t g_dac_cosine[256];

uint16_t g_dac_square_64[50] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095
};

void StartDACProcessingTask(void *argument) {
    // 启动定时器
    HAL_TIM_Base_Start(&htim4);
    
    // 首次生成波形数据
    update_dac_waveform_by_parameters();
    
    // 启动DAC DMA传输
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_square_64, 50, DAC_ALIGN_12B_R);

    // 设置DAC通道2的固定电压输出
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2
    
    for(;;)
    {
        
        osDelay(100); // 延时100毫秒
    }
}

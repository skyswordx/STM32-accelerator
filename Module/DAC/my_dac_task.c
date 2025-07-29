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

void StartDACProcessingTask(void *argument) {
    // 启动定时器
    HAL_TIM_Base_Start(&htim4);
    
    // 首次生成波形数据
    update_dac_waveform_by_parameters();
    
    // 启动DAC DMA传输
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_sine, 256, DAC_ALIGN_12B_R);
    
    // 设置DAC通道2的固定电压输出
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2
    
    for(;;)
    {
        // 检查参数是否发生变化，如果变化则更新波形
        static uint8_t last_waveform = 255;
        static uint32_t last_frequency = 0;
        static float32_t last_amplitude = -1.0f;
        
        if (last_waveform != g_desired_DAC_output_waveform ||
            last_frequency != g_desired_DAC_output_frequency ||
            last_amplitude != g_desired_DAC_single_output_amplitude) {
            
            // 更新缓存值
            last_waveform = g_desired_DAC_output_waveform;
            last_frequency = g_desired_DAC_output_frequency;
            last_amplitude = g_desired_DAC_single_output_amplitude;
            
            // 停止当前DMA传输
            HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
            
            // // 重新生成波形数据
            update_dac_waveform_by_parameters();
            
            // // 重新启动DAC DMA传输
            HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_waveform_buffer, 256, DAC_ALIGN_12B_R);
            
            // // 更新DAC通道2的电压输出
            // HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
            
            printf("DAC waveform updated: Waveform=%d, Frequency=%lu Hz, Amplitude=%.2f V\n",
                   g_desired_DAC_output_waveform, g_desired_DAC_output_frequency, g_desired_DAC_single_output_amplitude);
        }
        
        osDelay(100); // 延时100毫秒
    }
}

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

DDS_Generator_t g_dds_generator; // 宣告一個DDS產生器實例
#define WAVE_TABLE_SIZE 64
const uint16_t g_arbitrary_waveform[WAVE_TABLE_SIZE] = {
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

#define DDS_UPDATE_FREQUENCY 995062

void StartDACProcessingTask(void *argument) {

    // // 启动定时器
    // HAL_TIM_Base_Start(&htim4);
    
    // // 启动DAC DMA传输
    // HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_square_64, 50, DAC_ALIGN_12B_R);

    printf("Initializing DDS generator...\r\n");
    // 1. 初始化DDS產生器
    DDS_Init(&g_dds_generator, &hdac1, DAC_CHANNEL_1, &htim4, DDS_UPDATE_FREQUENCY);

    printf("Setting waveform...\r\n");
    // 2. 設定要使用的波形
    DDS_SetWaveform(&g_dds_generator, g_arbitrary_waveform, WAVE_TABLE_SIZE);

    printf("Setting frequency...\r\n");
    // 3. 設定初始輸出頻率，例如 1000.5 Hz
    DDS_SetFrequency(&g_dds_generator, 1000.5f);

    printf("Starting DDS...\r\n");
    // 4. 啟動DDS引擎（這會自動啟動定時器和DMA）
    DDS_Start(&g_dds_generator);
    printf("DDS started.\r\n");

    // // 设置DAC通道2的固定电压输出
    // HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    // HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2
    
    for(;;)
    {
        
        osDelay(100); // 延时100毫秒
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
  // 這是DMA傳輸完整個緩衝區後的回呼 (Pong區完成)
  DDS_Callback_FullTransfer(&g_dds_generator);
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
  // 這是DMA傳輸完前半個緩衝區後的回呼 (Ping區完成)
  DDS_Callback_HalfTransfer(&g_dds_generator);
}
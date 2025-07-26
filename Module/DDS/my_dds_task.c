#include "my_dds_task.h"



void StartDDSProcessingTask(void const * argument)
{
    // DDS任务代码
    // 这里可以添加DDS相关的初始化和处理逻辑

    // AD9954 DDS
    AD9954_Init();
    AD9954_Set_Fre(1000.0);
    AD9954_Set_Amp(16383);
    AD9954_Set_Phase(0);

    // AD9833 DDS
    AD9833_Init_GPIO();
    AD9833_WaveSeting(1000.0, 0, SIN_WAVE, 0); // 设置AD9833为正弦波，频率1kHz，初相位0
    AD9833_AmpSet(0x1FFF); // 设置AD9833幅度为最大值

    uint16_t delta_frequency = 1000; // 频率增量

    for(;;)
    {
        AD9954_Set_Fre(delta_frequency); // 设置频率为1kHz
        AD9833_WaveSeting(delta_frequency, 0, SIN_WAVE, 0); // 设置AD9833频率为1kHz
        // printf("DDS Frequency set to: %lu Hz\n", delta_frequency);
        delta_frequency += 1000; // 每次增加1kHz
        if (delta_frequency > 10000000) { // 如果超过10MHz，重置为1kHz
            delta_frequency = 1000;
        }
        osDelay(5000); // 延时5s
    }
}
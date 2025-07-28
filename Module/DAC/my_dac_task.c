#include "my_dac_task.h"

extern TIM_HandleTypeDef htim4;
extern const uint16_t ch1_value[256];
extern const uint16_t ch2_value[256];

extern float32_t g_desired_DAC_single_output_amplitude; // 默认DAC输出幅度为0.5

// DAC参考电压为3.3V，12位精度(0~4095)
#define DAC_MAX_VALUE 4095
#define DAC_REF_VOLTAGE 3.3f
#define VOLTAGE_TO_DAC_VALUE(voltage) ((uint16_t)((voltage) * DAC_MAX_VALUE / DAC_REF_VOLTAGE))

/**
 * 需求：
 * 可控波形（正弦波、方波、三角波等）
 * 可控相位
 * 可控输出波形频率
 * - 输出波形的频率 = 定时器触发频率 / 波形点数
 * - 可以结合定时器触发频率、灵活调整波形点数
 */

extern DAC_HandleTypeDef hdac1;

void StartDACProcessingTask(void *argument) {

    HAL_TIM_Base_Start(&htim4);


    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)ch1_value, 256, DAC_ALIGN_12B_R);
    // HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t *)ch2_value, 256, DAC_ALIGN_12B_R);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2
    for (;;) {
        // Process DAC channels
        
        osDelay(1);
    }
}

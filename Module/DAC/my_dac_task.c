#include "my_dac_task.h"

extern TIM_HandleTypeDef htim4;
extern const uint16_t ch1_value[256];
extern const uint16_t ch2_value[256];

extern DAC_HandleTypeDef hdac1;

void StartDACProcessingTask(void *argument) {

    HAL_TIM_Base_Start(&htim4);

    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)ch1_value, 256, DAC_ALIGN_12B_R);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t *)ch2_value, 256, DAC_ALIGN_12B_R);
    for (;;) {
        // Process DAC channels
        
        osDelay(1);
    }
}

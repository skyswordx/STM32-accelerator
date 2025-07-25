#include "my_zlcr_task.h"

extern fundamental_result_t g_ch1_fundamental; // ADC1 通道 基波结果结构
extern fundamental_result_t g_ch2_fundamental; // ADC2 通道 基波结果结构
extern uint8_t g_all_fundamental_update_flag; // 所有基波更新标志

#define ZLCR_LENGTH 5

float32_t zlcr_magnitude[ZLCR_LENGTH]; // ZLCR幅值数组
float32_t zlcr_phase[ZLCR_LENGTH]; // ZLCR相位数组

void StartZLCRProcessingTask(void *argument) {
    
    uint32_t idx = 0;
    for (;;) {
        // 处理ZLCR数据
        if (g_all_fundamental_update_flag) {
            g_all_fundamental_update_flag = 0; // 重置所有基波更新标志


            zlcr_magnitude[idx] = (g_ch1_fundamental.fundamental_vrms/ g_ch2_fundamental.fundamental_vrms);
            zlcr_phase[idx] = g_ch1_fundamental.fundamental_phase_angle - g_ch2_fundamental.fundamental_phase_angle;
            idx++;

            if (idx >= ZLCR_LENGTH) {
                idx = 0; // 重置索引
    
                // 这里可以添加处理完一轮ZLCR数据后的逻辑，比如发送数据到其他任务或存储
                // 例如打印ZLCR数据
                for (uint32_t i = 0; i < ZLCR_LENGTH; i++) {
                    printf("ZLCR[%lu]: Magnitude: %.6f, Phase: %.2f\n", i, zlcr_magnitude[i], zlcr_phase[i]);
                }
            }
        }
        osDelay(100);
    }
}
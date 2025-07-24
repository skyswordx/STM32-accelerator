#ifndef MY_FREQ_CONFIG_H
#define MY_FREQ_CONFIG_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"
#include "my_adc_task.h"

#define FFT_LENGTH (4096) // FFT点数
typedef struct {
    float32_t fundamental_vpp;           // 基波峰峰值
    float32_t fundamental_vrms;          // 基波有效值
    uint16_t fundamental_frequency;       // 基波频率
    float32_t fundamental_phase_angle;  // 基波相位（角度）
} fundamental_result_t;

void my_armcfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, uint8_t enable_window);
void my_armrfft32_apply(float32_t* adc_input, fundamental_result_t* result, uint8_t enable_fir, uint8_t enable_window);

/* FIR 低通滤波器 + 窗函数 */
#define FIR_ORDER   100
#define NUM_TAPS    (FIR_ORDER + 1) // number of taps

#define HANNING_WINDOW_FACTOR 2.325475f // 汉宁窗补偿系数



#endif /* MY_FREQ_CONFIG_H */
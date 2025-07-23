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

void my_armcfft32_apply(float32_t* adc_input, const arm_cfft_radix4_instance_f32* fft_instance, fundamental_result_t* result);

/* FIR 低通滤波器 + 窗函数 */
#define FIR_ORDER   100
#define NUM_TAPS    (FIR_ORDER + 1)


#endif /* MY_FREQ_CONFIG_H */
#ifndef MY_FIR_WINDOWS_CONFIG_H
#define MY_FIR_WINDOWS_CONFIG_H

#include "arm_math.h"

#define FIR_FILTER_ORDER 32
#define FIR_TAP_NUM (FIR_FILTER_ORDER + 1)

extern float32_t g_fir_coefficients[FIR_TAP_NUM];
extern float32_t g_fir_state[FIR_TAP_NUM];

void my_fir_filter_init(void);
void my_fir_filter(float32_t* input, float32_t* output, uint32_t length);

#endif // MY_FIR_WINDOWS_CONFIG_H
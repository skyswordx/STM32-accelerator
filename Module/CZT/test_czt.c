#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "my_czt_config.h"

#define TEST_N 16
#define TEST_M 16

// 测试信号生成函数
void generate_test_signal(float32_t* real, float32_t* imag, int length, float32_t frequency) {
    for (int i = 0; i < length; i++) {
        real[i] = arm_cos_f32(2 * M_PI * frequency * i / length);
        imag[i] = arm_sin_f32(2 * M_PI * frequency * i / length);
    }
}

// 打印复数数组
void print_complex_array(const char* name, const float32_t* real, const float32_t* imag, int length) {
    printf("%s:\n", name);
    for (int i = 0; i < length; i++) {
        printf("  [%d]: %f + j%f\n", i, real[i], imag[i]);
    }
    printf("\n");
}

int main(void) {
    // 修改配置参数以适应测试
    #undef CZT_N
    #undef CZT_M
    #define CZT_N TEST_N
    #define CZT_M TEST_M
    
    float32_t input_real[CZT_N];
    float32_t input_imag[CZT_N];
    float32_t output_real[CZT_M];
    float32_t output_imag[CZT_M];
    
    printf("Chirp Z-Transform (CZT) Test\n");
    printf("============================\n");
    
    // 初始化CZT模块
    int ret = my_czt_init();
    if (ret != MY_CZT_SUCCESS) {
        printf("CZT initialization failed with error code: %d\n", ret);
        return -1;
    }
    printf("CZT module initialized successfully.\n\n");
    
    // 生成测试信号 (一个简单的正弦波)
    generate_test_signal(input_real, input_imag, CZT_N, 2.0f);
    printf("Generated test signal (complex exponential with frequency 2.0):\n");
    print_complex_array("Input", input_real, input_imag, CZT_N);
    
    // 执行CZT处理
    ret = my_czt_process(input_real, input_imag, output_real, output_imag);
    if (ret != MY_CZT_SUCCESS) {
        printf("CZT processing failed with error code: %d\n", ret);
        return -1;
    }
    printf("CZT processing completed successfully.\n");
    
    // 打印输出结果
    print_complex_array("Output", output_real, output_imag, CZT_M);
    
    printf("Test completed.\n");
    return 0;
}
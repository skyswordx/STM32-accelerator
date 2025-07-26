#include "my_quinn_config.h"
#include "my_freq_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// 测试参数
#define TEST_FFT_LENGTH 4096
#define TEST_SAMPLE_RATE 500000  // 500kHz
#define PI 3.14159265358979323846

// 模拟FFT数据
float32_t test_fft_data[TEST_FFT_LENGTH * 2];

// 生成测试信号
void generate_test_signal(float32_t frequency, float32_t phase, uint32_t sample_rate, uint32_t length) {
    // 清零FFT数据
    memset(test_fft_data, 0, sizeof(test_fft_data));
    
    // 生成复正弦信号
    for (uint32_t i = 0; i < length; i++) {
        float32_t t = (float32_t)i / sample_rate;
        float32_t angle = 2.0f * PI * frequency * t + phase;
        test_fft_data[2 * i] = cosf(angle);     // 实部
        test_fft_data[2 * i + 1] = sinf(angle); // 虚部
    }
}

// 执行FFT获取峰值点
uint16_t find_peak_index() {
    float32_t max_magnitude = 0.0f;
    uint16_t peak_index = 0;
    
    // 计算幅度谱并找到峰值
    for (uint16_t i = 1; i < TEST_FFT_LENGTH / 2 - 1; i++) {
        float32_t real = test_fft_data[2 * i];
        float32_t imag = test_fft_data[2 * i + 1];
        float32_t magnitude = sqrtf(real * real + imag * imag);
        
        if (magnitude > max_magnitude) {
            max_magnitude = magnitude;
            peak_index = i;
        }
    }
    
    return peak_index;
}

// 测试Quinn算法
void test_quinn_algorithm() {
    printf("=== Quinn频率估计算法测试 ===\n");
    
    // 设置全局采样率
    extern uint32_t g_ADC_SAMPLE_RATE_Hz;
    g_ADC_SAMPLE_RATE_Hz = TEST_SAMPLE_RATE;
    
    // 测试频率列表
    float32_t test_frequencies[] = {1000.0f, 5000.0f, 10000.0f, 25000.0f, 50000.0f};
    uint32_t num_tests = sizeof(test_frequencies) / sizeof(test_frequencies[0]);
    
    printf("测试信号采样率: %d Hz\n", TEST_SAMPLE_RATE);
    printf("FFT点数: %d\n", TEST_FFT_LENGTH);
    printf("\n");
    
    // 启用Quinn模块
    g_quinn_module_enabled = 1;
    
    for (uint32_t i = 0; i < num_tests; i++) {
        float32_t test_freq = test_frequencies[i];
        float32_t test_phase = 0.0f;  // 初始相位
        
        printf("测试 %d: 频率 = %.1f Hz\n", i + 1, test_freq);
        
        // 生成测试信号
        generate_test_signal(test_freq, test_phase, TEST_SAMPLE_RATE, TEST_FFT_LENGTH);
        
        // 找到峰值点
        uint16_t peak_index = find_peak_index();
        float32_t coarse_freq = (float32_t)peak_index * TEST_SAMPLE_RATE / TEST_FFT_LENGTH;
        
        printf("  FFT峰值索引: %d\n", peak_index);
        printf("  粗略频率估计: %.2f Hz\n", coarse_freq);
        
        // 应用Quinn算法
        quinn_frequency_result_t quinn_result;
        int status = my_quinn_process(test_fft_data, peak_index, &quinn_result);
        
        if (status == MY_QUINN_SUCCESS) {
            printf("  Quinn算法执行: 成功\n");
            printf("  精确频率估计: %.2f Hz\n", quinn_result.frequency);
            printf("  频率偏移量: %.6f\n", quinn_result.delta);
            printf("  结果有效性: %s\n", quinn_result.validity ? "有效" : "无效");
            printf("  结果可信度: %.2f\n", quinn_result.confidence);
            
            // 计算误差
            float32_t error = fabsf(quinn_result.frequency - test_freq);
            float32_t relative_error = error / test_freq * 100.0f;
            printf("  频率估计误差: %.6f Hz (%.4f%%)\n", error, relative_error);
        } else {
            printf("  Quinn算法执行: 失败 (错误码: %d)\n", status);
        }
        
        printf("\n");
    }
    
    // 测试禁用Quinn模块的情况
    printf("=== 测试禁用Quinn模块 ===\n");
    g_quinn_module_enabled = 0;
    
    generate_test_signal(10000.0f, 0.0f, TEST_SAMPLE_RATE, TEST_FFT_LENGTH);
    uint16_t peak_index = find_peak_index();
    
    quinn_frequency_result_t quinn_result;
    int status = my_quinn_process(test_fft_data, peak_index, &quinn_result);
    
    if (status == MY_QUINN_ERROR_PROCESS_FAILED) {
        printf("Quinn模块已禁用，算法正确返回失败状态\n");
    } else {
        printf("错误：禁用状态下应返回失败状态\n");
    }
    
    printf("\n测试完成。\n");
}

int main() {
    test_quinn_algorithm();
    return 0;
}
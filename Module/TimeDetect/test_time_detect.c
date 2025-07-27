#include "my_time_detect.h"
#include "my_freq_config.h"  // 引入频域处理模块
#include "main.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// 生成测试信号
void generate_test_signal(float32_t* signal, uint32_t length, uint32_t sample_rate, float32_t frequency, int waveform_type) {
    for (uint32_t i = 0; i < length; i++) {
        float32_t t = (float32_t)i / sample_rate; // 时间
        switch (waveform_type) {
            case WAVEFORM_SINE:
                // 正弦波
                signal[i] = 1.0f * arm_sin_f32(2.0f * PI * frequency * t);
                break;
            case WAVEFORM_SQUARE:
                // 方波
                signal[i] = (arm_sin_f32(2.0f * PI * frequency * t) >= 0.0f) ? 1.0f : -1.0f;
                break;
            case WAVEFORM_TRIANGLE:
                // 三角波
                signal[i] = (2.0f / PI) * arm_asin_f32(arm_sin_f32(2.0f * PI * frequency * t));
                break;
            default:
                signal[i] = 0.0f;
                break;
        }
        // 添加一些噪声和直流分量
        signal[i] += 0.1f * ((float32_t)rand() / RAND_MAX - 0.5f) + 0.5f;
    }
}

// 测试时域检测模块
void test_time_detect_module() {
    printf("Starting Time Domain Detection Module Test...\n");
    
    // 测试参数
    const uint32_t SAMPLE_RATE = 10000; // 10kHz采样率
    const uint32_t DATA_LENGTH = 1024;  // 1024个点
    const float32_t TEST_FREQUENCY = 100.0f; // 100Hz测试频率
    
    // 分配内存
    float32_t* test_signal = (float32_t*)malloc(DATA_LENGTH * sizeof(float32_t));
    if (test_signal == NULL) {
        printf("Failed to allocate memory for test signal!\n");
        return;
    }
    
    // 测试不同波形
    const char* waveform_names[] = {"Unknown", "Sine", "Square", "Triangle"};
    int waveform_types[] = {WAVEFORM_SINE, WAVEFORM_SQUARE, WAVEFORM_TRIANGLE};
    
    for (int i = 0; i < 3; i++) {
        printf("\n--- Testing %s Waveform ---\n", waveform_names[waveform_types[i]]);
        
        // 生成测试信号
        generate_test_signal(test_signal, DATA_LENGTH, SAMPLE_RATE, TEST_FREQUENCY, waveform_types[i]);
        
        // 初始化时域检测配置
        time_detect_config_params_t time_config;
        time_config.sample_rate = SAMPLE_RATE;
        time_config.data_length = DATA_LENGTH;
        time_config.enable_dc_filter = 1; // 启用直流分量滤除
        
        // 初始化时域检测模块
        my_time_detect_init(&time_config);
        
        // 启动时域检测
        time_detect_result_t result;
        int ret = my_time_detect_start(test_signal, &result);
        
        if (ret == 0) {
            // 输出检测结果
            printf("Detection Results:\n");
            printf("  DC Component: %.6f V\n", result.dc_component);
            printf("  RMS Value: %.6f V\n", result.rms_value);
            printf("  Fundamental Frequency: %lu Hz\n", result.fundamental_freq);
            printf("  Period Points: %lu\n", result.period_points);
            printf("  Waveform Ratio: %.6f\n", result.waveform_ratio);
            printf("  Detected Waveform: %s\n", waveform_names[result.waveform_type]);
            
            // 验证结果
            if (result.fundamental_freq > 0) {
                float32_t freq_error = fabsf((float32_t)result.fundamental_freq - TEST_FREQUENCY) / TEST_FREQUENCY * 100.0f;
                printf("  Frequency Error: %.2f%%\n", freq_error);
            }
        } else {
            printf("Time domain detection failed with code %d!\n", ret);
        }
    }
    
    // 释放内存
    free(test_signal);
    
    printf("\nTime Domain Detection Module Test Completed.\n");
}

int main(void) {
    // HAL库初始化
    HAL_Init();
    
    // 配置系统时钟
    SystemClock_Config();
    
    // 初始化外设
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    
    // 重定向printf到串口
    // 注意：需要在main.h中声明相关函数
    
    // 运行测试
    test_time_detect_module();
    
    while (1) {
        // 主循环
        HAL_Delay(1000);
    }
}
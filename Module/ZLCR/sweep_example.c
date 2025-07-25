#include "my_zlcr_config.h"
#include <stdio.h>

// 扫频使用示例
void sweep_example() {
    // 示例1: 快速概览扫描
    printf("=== 快速概览扫描 ===\n");
    sweep_config_t quick_config = {
        .type = SWEEP_LOGARITHMIC,
        .start_freq = 1000,     // 1kHz
        .stop_freq = 1000000,   // 1MHz
        .strategy = STRATEGY_QUICK_VIEW
    };
    
    // 设置默认点数
    set_default_points_by_strategy(&quick_config);
    
    // 生成频率点
    generate_frequency_points(&quick_config);
    
    // 输出生成的频率点
    printf("生成了 %lu 个频率点:\n", (unsigned long)quick_config.points);
    for (uint32_t i = 0; i < quick_config.points && i < 10; i++) {
        printf("  频率点 %lu: %lu Hz\n", (unsigned long)(i+1), (unsigned long)quick_config.freq_array[i]);
    }
    if (quick_config.points > 10) {
        printf("  ... (还有 %lu 个点)\n", (unsigned long)(quick_config.points - 10));
    }
    
    // 示例2: 标准特性表征扫描
    printf("\n=== 标准特性表征扫描 ===\n");
    sweep_config_t standard_config = {
        .type = SWEEP_LOGARITHMIC,
        .start_freq = 1000,     // 1kHz
        .stop_freq = 10000000,  // 10MHz
        .strategy = STRATEGY_STANDARD
    };
    
    // 设置默认点数
    set_default_points_by_strategy(&standard_config);
    
    // 生成频率点
    generate_frequency_points(&standard_config);
    
    // 输出生成的频率点
    printf("生成了 %lu 个频率点:\n", standard_config.points);
    for (uint32_t i = 0; i < standard_config.points && i < 10; i++) {
        printf("  频率点 %lu: %lu Hz\n", i+1, standard_config.freq_array[i]);
    }
    if (standard_config.points > 10) {
        printf("  ... (还有 %lu 个点)\n", standard_config.points - 10);
    }
    
    // 示例3: 精细分析扫描
    printf("\n=== 精细分析扫描 ===\n");
    sweep_config_t fine_config = {
        .type = SWEEP_LINEAR,
        .start_freq = 900000,   // 900kHz
        .stop_freq = 1100000,   // 1.1MHz
        .strategy = STRATEGY_FINE_ANALYSIS
    };
    
    // 设置默认点数
    set_default_points_by_strategy(&fine_config);
    
    // 生成频率点
    generate_frequency_points(&fine_config);
    
    // 输出生成的频率点
    printf("生成了 %lu 个频率点:\n", fine_config.points);
    for (uint32_t i = 0; i < fine_config.points && i < 10; i++) {
        printf("  频率点 %lu: %lu Hz\n", i+1, fine_config.freq_array[i]);
    }
    if (fine_config.points > 10) {
        printf("  ... (还有 %lu 个点)\n", fine_config.points - 10);
    }
    
    // 示例4: 谐振点查找扫描
    printf("\n=== 谐振点查找扫描 ===\n");
    sweep_config_t resonant_config = {
        .type = SWEEP_LOGARITHMIC,
        .start_freq = 100000,   // 100kHz
        .stop_freq = 5000000,   // 5MHz
        .strategy = STRATEGY_RESONANT_SEARCH
    };
    
    // 设置默认点数
    set_default_points_by_strategy(&resonant_config);
    
    // 生成频率点
    generate_frequency_points(&resonant_config);
    
    // 输出生成的频率点
    printf("生成了 %lu 个频率点:\n", resonant_config.points);
    for (uint32_t i = 0; i < resonant_config.points && i < 10; i++) {
        printf("  频率点 %lu: %lu Hz\n", i+1, resonant_config.freq_array[i]);
    }
    if (resonant_config.points > 10) {
        printf("  ... (还有 %lu 个点)\n", resonant_config.points - 10);
    }
}

int main() {
    sweep_example();
    return 0;
}
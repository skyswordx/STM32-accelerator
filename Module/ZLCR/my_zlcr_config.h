#ifndef MY_ZLCR_TASK_H
#define MY_ZLCR_TASK_H

#include "cmsis_os.h"
#include "main.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "arm_math.h"
#include "my_adc_task.h"
#include "my_freq_config.h"

// 扫频类型定义
typedef enum {
    SWEEP_LINEAR,     // 线性扫描
    SWEEP_LOGARITHMIC // 对数扫描
} sweep_type_t;

// 扫描策略定义
typedef enum {
    STRATEGY_QUICK_VIEW,      // 快速概览 (10-20点)
    STRATEGY_STANDARD,        // 标准特性表征 (50-200点)
    STRATEGY_FINE_ANALYSIS,   // 精细分析 (200-1000点)
    STRATEGY_RESONANT_SEARCH // 谐振点查找 (分两步扫描)
} sweep_strategy_t;

// 扫频配置结构体
typedef struct {
    sweep_type_t type;           // 扫描类型
    uint32_t start_freq;         // 起始频率(Hz)
    uint32_t stop_freq;          // 终止频率(Hz)
    uint32_t points;             // 扫描点数
    sweep_strategy_t strategy;   // 扫描策略
    uint32_t* freq_array;        // 频率点数组指针
    uint32_t array_length;       // 频率点数组长度
} sweep_config_t;

// 扫频结果结构体
typedef struct {
    uint32_t frequency;    // 频率(Hz)
    float32_t magnitude;   // 幅值
    float32_t phase;       // 相位(度)
    float32_t impedance;   // 阻抗(欧姆)
} sweep_point_result_t;


// 扫频功能函数声明
// 生成频率点数组
void generate_frequency_points(sweep_config_t* config);

// 根据扫描策略设置默认点数
void set_default_points_by_strategy(sweep_config_t* config);

// 设置DDS频率的接口函数声明
void set_dds_frequency(double frequency);

// 设置DDS实际模式的接口函数声明
void set_dds_mode(uint16_t mode);

#endif // MY_ZLCR_TASK_H
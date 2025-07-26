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

// 扫频配置参数
#define SWEEP_MAX_POINTS 1024  // 最大扫频点数

typedef struct {
    uint32_t frequency;    // 频率(Hz)
    float32_t magnitude;   // 幅值(欧姆)
    float32_t phase;       // 相位(度)
} sweep_point_result_t;

// DDS 设备抽象结构体
typedef struct {
    void (*init)(void);                           // 初始化函数指针
    void (*set_frequency)(double frequency);       // 设置频率函数指针
} dds_device_t;

// 扫频模式枚举
typedef enum {
    SWEEP_MODE_LINEAR = 0,    // 线性扫频
    SWEEP_MODE_LOG,           // 对数扫频
    SWEEP_MODE_DECADE         // 十倍频扫频
} sweep_mode_t;

// 扫频配置结构体
typedef struct {
    uint32_t start_freq;      // 起始频率(Hz)
    uint32_t stop_freq;       // 终止频率(Hz)
    uint32_t points;          // 扫频点数
    sweep_mode_t mode;        // 扫频模式
    uint32_t points_per_decade; // 每十倍频点数(仅在十倍频扫频模式下使用)
} sweep_config_t;

// 全局变量声明
extern uint32_t g_sweep_freq_array[SWEEP_MAX_POINTS];     // 扫频频率数组
extern sweep_point_result_t g_sweep_result_array[SWEEP_MAX_POINTS]; // 扫频结果数组
extern uint32_t g_sweep_current_index;                    // 当前扫频点索引
extern uint32_t g_sweep_total_points;                     // 总扫频点数
extern dds_device_t g_dds_device;                         // DDS设备实例

void my_zlcr_get_impedance(const fundamental_result_t *ch1_fundamental, const fundamental_result_t *ch2_fundamental,
                           sweep_point_result_t *current_freq_result);

// 扫频控制函数
void my_zlcr_sweep_init(const sweep_config_t *config);
void my_zlcr_sweep_start(void);
void my_zlcr_sweep_next(void);
uint8_t my_zlcr_sweep_is_complete(void);

// DDS设备初始化函数
void my_zlcr_dds_init(void);

#endif // MY_ZLCR_TASK_H
#include "my_zlcr_config.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "AD9833.h"
#include "AD9954.h"
#define  PHASE_THRESHOLD 5.0f

// 全局频率数组，用于不同扫描场景
uint32_t g_sweep_freq_array[SWEEP_MAX_POINTS];           // 扫频频率数组
sweep_point_result_t g_sweep_result_array[SWEEP_MAX_POINTS]; // 扫频结果数组
uint32_t g_sweep_current_index = 0;                      // 当前扫频点索引
impedance_result_t g_current_impedance_result;          // 当前频率下的阻抗结果
uint32_t g_sweep_total_points = 0;                       // 总扫频点数

// DDS设备实例
dds_device_t g_dds_device;

#define Rx 220 // 模拟前端的电阻是220欧姆

sweep_point_result_t g_current_freq_result;

void my_zlcr_get_impedance(const fundamental_result_t *ch1_fundamental, const fundamental_result_t *ch2_fundamental,
                           sweep_point_result_t *current_freq_result) {
    // 计算阻抗
    /* (Vch2 / Rx) = (Vch1 / Rz) 借此反推 Rz 得到阻抗 */
    // 计算阻抗 Rz = (Rx * Vch1) / Vch2
    float32_t Vch1 = ch1_fundamental->fundamental_vrms; // ADC1通道的基波有效值
    float32_t Vch2 = ch2_fundamental->fundamental_vrms; // ADC2通道的基波有效值

    current_freq_result->magnitude = (Rx * Vch1) / Vch2;
    current_freq_result->phase = ((ch1_fundamental->fundamental_phase_angle - ch2_fundamental->fundamental_phase_angle) - 180.0f); // 相位差
    current_freq_result->frequency = (ch1_fundamental->fundamental_frequency + ch2_fundamental->fundamental_frequency) / 2.0f; // 频率

}

// 频率点生成函数 - 多十倍频扫描
void generate_decade_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points_per_decade, uint32_t* freq_array, uint32_t* generated_points) {
    // 计算起始和终止频率的对数
    uint32_t start_decade = (uint32_t)floorf(log10f((float)start_freq));
    uint32_t stop_decade = (uint32_t)ceilf(log10f((float)stop_freq));
    
    uint32_t total_points = 0;
    
    // 对每个十倍频程进行扫描
    for (uint32_t decade = start_decade; decade <= stop_decade; decade++) {
        uint32_t decade_start = (uint32_t)powf(10.0f, (float)decade);
        uint32_t decade_stop = decade_start * 10;
        
        // 调整边界
        if (decade_start < start_freq) decade_start = start_freq;
        if (decade_stop > stop_freq) decade_stop = stop_freq;
        
        // 在当前十倍频程内进行对数扫描
        if (decade_stop > decade_start) {
            uint32_t points_in_decade = points_per_decade;
            // 如果是第一个或最后一个十倍频程，可能需要调整点数
            if (decade == start_decade || decade == stop_decade) {
                float ratio = (float)(decade_stop - decade_start) / (float)(decade * 10 - decade_start);
                points_in_decade = (uint32_t)(points_per_decade * ratio);
                if (points_in_decade < 2) points_in_decade = 2;
            }
            
            // 生成当前十倍频程的频率点
            if (total_points + points_in_decade <= *generated_points) {
                generate_logarithmic_frequency_points(decade_start, decade_stop, points_in_decade, &freq_array[total_points]);
                total_points += points_in_decade;
            } else {
                // 如果数组空间不足，只生成部分点
                uint32_t remaining_points = *generated_points - total_points;
                if (remaining_points > 0) {
                    generate_logarithmic_frequency_points(decade_start, decade_stop, remaining_points, &freq_array[total_points]);
                    total_points += remaining_points;
                }
                break;
            }
        }
    }
    
    *generated_points = total_points;
}



// 频率点生成函数 - 线性扫描
void generate_linear_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points, uint32_t* freq_array) {
    if (points <= 1) {
        freq_array[0] = start_freq;
        return;
    }
    
    for (uint32_t i = 0; i < points; i++) {
        freq_array[i] = start_freq + i * (stop_freq - start_freq) / (points - 1);
    }
}

// 频率点生成函数 - 对数扫描
void generate_logarithmic_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points, uint32_t* freq_array) {
    if (points <= 1) {
        freq_array[0] = start_freq;
        return;
    }
    
    if (start_freq == 0) start_freq = 1; // 避免对数计算错误
    
    float log_start = logf((float)start_freq);
    float log_stop = logf((float)stop_freq);
    float log_step = (log_stop - log_start) / (points - 1);
    
    for (uint32_t i = 0; i < points; i++) {
        freq_array[i] = (uint32_t)(expf(log_start + i * log_step) + 0.5f); // 四舍五入
    }
}

// AD9833 DDS设备初始化函数
void ad9833_init(void) {
    AD9833_Init_GPIO();
}

// AD9833 DDS设备设置频率函数
void ad9833_set_frequency(double frequency) {
    // AD9833默认使用正弦波模式，相位为0
    AD9833_WaveSeting(frequency, 0, SIN_WAVE, 0);
}

// AD9954 DDS设备初始化函数
void ad9954_init(void) {
    AD9954_Init();
}

// AD9954 DDS设备设置频率函数
void ad9954_set_frequency(double frequency) {
    AD9954_Set_Fre(frequency);
}

// 初始化DDS设备
void my_zlcr_dds_init(uint8_t dds_type) {
    
    if (dds_type == DDS_TYPE_AD9833) {
        g_dds_device.init = ad9833_init;
        g_dds_device.set_frequency = ad9833_set_frequency;
    } else if (dds_type == DDS_TYPE_AD9954) {
        g_dds_device.init = ad9954_init;
        g_dds_device.set_frequency = ad9954_set_frequency;
    } else {
        printf("Unsupported DDS type!\n");
        return;
    }
    
    // 初始化DDS设备
    if (g_dds_device.init != NULL) {
        // g_dds_device.init();
        AD9833_Init_GPIO();
        AD9954_Init();
    }
}

// 初始化扫频配置
void my_zlcr_sweep_init(const sweep_config_t *config) {
    // 重置索引
    g_sweep_current_index = 0;
    g_sweep_total_points = 0;
    
    // 根据扫频模式生成频率点
    switch (config->mode) {
        case SWEEP_MODE_LINEAR:
            g_sweep_total_points = config->points;
            if (g_sweep_total_points > SWEEP_MAX_POINTS) {
                g_sweep_total_points = SWEEP_MAX_POINTS;
            }
            generate_linear_frequency_points(config->start_freq, config->stop_freq, g_sweep_total_points, g_sweep_freq_array);
            break;
            
        case SWEEP_MODE_LOG:
            g_sweep_total_points = config->points;
            if (g_sweep_total_points > SWEEP_MAX_POINTS) {
                g_sweep_total_points = SWEEP_MAX_POINTS;
            }
            generate_logarithmic_frequency_points(config->start_freq, config->stop_freq, g_sweep_total_points, g_sweep_freq_array);
            break;
            
        case SWEEP_MODE_DECADE:
            g_sweep_total_points = SWEEP_MAX_POINTS; // 先设置为最大值
            generate_decade_frequency_points(config->start_freq, config->stop_freq, config->points_per_decade, g_sweep_freq_array, &g_sweep_total_points);
            break;
            
        default:
            // 默认使用线性扫频
            g_sweep_total_points = config->points;
            if (g_sweep_total_points > SWEEP_MAX_POINTS) {
                g_sweep_total_points = SWEEP_MAX_POINTS;
            }
            generate_linear_frequency_points(config->start_freq, config->stop_freq, g_sweep_total_points, g_sweep_freq_array);
            break;
    }
}

// 开始扫频
void my_zlcr_sweep_start(void) {
    // 重置索引
    g_sweep_current_index = 0;
    
    // 如果有频率点，则设置第一个频率
    if (g_sweep_total_points > 0 && g_dds_device.set_frequency != NULL) {
        g_dds_device.set_frequency((double)g_sweep_freq_array[0]);
    }
}

// 切换到下一个频率点
void my_zlcr_sweep_next(void) {
    // 增加索引
    g_sweep_current_index++;
    
    // 如果还没到末尾，则设置下一个频率
    if (g_sweep_current_index < g_sweep_total_points && g_dds_device.set_frequency != NULL) {
        g_dds_device.set_frequency((double)g_sweep_freq_array[g_sweep_current_index]);
    }
}

// 检查扫频是否完成
uint8_t my_zlcr_sweep_is_complete(void) {
    return (g_sweep_current_index >= g_sweep_total_points) ? 1 : 0;
}


/**
 * @brief 根据扫频点结果计算电容或电感值，ESR 以及 Q 值或者 D 值
 * @param result 扫频点测量结果
 *
 * @details
 * 假设现在测得的阻抗是纯电容/纯电感，此时的幅值就为虚部的绝对值，利用公式反推 C 与 L 的值
 * - 对于电容: Zc = 1 / (2 * π * f * C) => C = 1 / (2 * π * f * |Z|)
 * - 对于电感: Zl = 2 * π * f * L => L = |Z| / (2 * π * f)
 * - sweep_point_result_t 中的 magnitude 是 A*e^(j*phase) 的 A 部分，即幅值
 * - phase 是相位角度，单位为度
 * @param impedance_result 输出的阻抗结果，包括等效串联电阻(ESR)、电容或电感值以及质量因子(Q值)
 * 
 */
void my_zlcr_get_capacitance_or_inductance(const sweep_point_result_t *result, impedance_result_t *impedance_result) {

    if (result == NULL || impedance_result == NULL) {
        return;
    }

    // 计算阻抗的幅值和相位
    float32_t magnitude = result->magnitude;
    float32_t phase = result->phase;

    // 根据相位判断是电容还是电感
    if (phase < (-90.0f + PHASE_THRESHOLD)) {
        // 纯电容
        printf("Pure Capacitor Detected\n");
        impedance_result->c_or_l = 1.0f / (2.0f * PI * result->frequency * magnitude);
    } else if (phase > (90.0f - PHASE_THRESHOLD)) {
        // 纯电感
        printf("Pure Inductor Detected\n");
        impedance_result->c_or_l = magnitude / (2.0f * PI * result->frequency);
    } else {
        // 其他情况
        impedance_result->c_or_l = 0.0f;
    }

    // 计算等效串联电阻和质量因子
    impedance_result->esr = magnitude * cosf(phase * PI / 180.0f);
    impedance_result->quality_factor = magnitude / impedance_result->esr;
    printf("ESR: %.2f Ohm, C/L: %.6f F/H, Q: %.2f\n", impedance_result->esr, impedance_result->c_or_l, impedance_result->quality_factor);
   
}


/**
* 
有个别电桥可能副参数显示的种类不够全。比如有的只能显示D值，Q值，显示不了ESR。
实际上这三个参数知道一个，另外两个通过公式就能算出来。D值和Q值互为倒数，这个最简单！ESR和D值Q值换算稍微复杂点。网上查了一下，公式如下。

公式一：|Z| = 1 / ( 2πf C)

其中，|Z|为电抗的绝对值，单位Ω；f为频率，单位Hz；C为容量，单位元F。

公式二：Q = |Z| / ESR

其中，Q代表“质量因素”，无量纲；|Z|为电抗的绝对值，单位Ω；ESR为等效串联电阻，单位Ω。

两个公式合并可以得到：
Q值=1/2πfc*ESR
D值=2πfc*ESR
 */
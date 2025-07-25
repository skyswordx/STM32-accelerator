
#ifndef MY_PID_CONFIG_H
#define MY_PID_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

// 卡尔曼滤波器结构体
typedef struct {
    float q; // 过程噪声协方差
    float r; // 测量噪声协方差
    float x; // 状态估计值
    float p; // 估计误差协方差
    float k; // 卡尔曼增益
    bool initialized; // 初始化标志
} KalmanFilter_t;

void KalmanFilter_Init(KalmanFilter_t *kf, float process_noise, float measurement_noise);
float KalmanFilter_Update(KalmanFilter_t *kf, float measurement);
void KalmanFilter_Reset(KalmanFilter_t *kf);
void KalmanFilter_SetProcessNoise(KalmanFilter_t *kf, float process_noise);
void KalmanFilter_SetMeasurementNoise(KalmanFilter_t *kf, float measurement_noise);

// PID 控制器过程变量
typedef struct {
    float measure;
    float last_measure;
    float last_output;
    float target;
    float error;
} process_data_t;

// PID 控制器结构体
typedef struct {
    process_data_t process_variable;
    float controller_output;
    float CONTROLLER_OUTPUT_MAX;
    float CONTROLLER_OUTPUT_MIN;
    float kp, ki, kd;
    KalmanFilter_t kalman_filter;
    bool enable_kalman_filter;
    float (*read_sensor)(void); // 读取传感器数据的函数指针
    float (*convert_output)(float); // 转换控制器输出的函数指针
} PID_controller_t;

void PID_controller_init(PID_controller_t *pid, float output_max, float output_min, float kp, float ki, float kd);
void PID_controller_set_kalmanFilter(PID_controller_t *pid, float process_noise, float measurement_noise);
void PID_controller_enable_kalmanFilter(PID_controller_t *pid, bool enable);
void PID_controller_start(PID_controller_t *pid);

/*
================== PID 控制器裸机使用示例 ==================

#include "my_pid_config.h"

// 用户自定义传感器读取函数
float read_sensor_example(void) {
    // 这里返回实际测量值，例如 ADC 采集结果
    return 42.0f;
}

// 用户自定义输出转换函数
float convert_output_example(float output) {
    // 这里可以做 DAC 输出或其他硬件操作
    return output;
}

int main(void) {
    PID_controller_t pid;
    PID_controller_init(&pid, 3.3f, 0.0f, 1.0f, 0.1f, 0.01f); // 最大/最小输出，PID参数
    pid.read_sensor = read_sensor_example;
    pid.convert_output = convert_output_example;
    pid.enable_kalman_filter = false; // 不使用卡尔曼滤波

    pid.process_variable.target = 50.0f; // 设置目标值

    while (1) {
        PID_controller_start(&pid); // 计算并更新输出
        // 可在此处使用 pid.controller_output 进行硬件控制
    }
    return 0;
}
============================================================
*/

#endif /* MY_PID_CONFIG_H */


#ifndef MY_KALMAN_FILTER_H
#define MY_KALMAN_FILTER_H

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
} kalman_filter_t;

void kalman_filter_init(kalman_filter_t *kf, float process_noise, float measurement_noise);
float kalman_filter_update(kalman_filter_t *kf, float measurement);
void kalman_filter_reset(kalman_filter_t *kf);
void kalman_filter_set_process_noise(kalman_filter_t *kf, float process_noise);
void kalman_filter_set_measurement_noise(kalman_filter_t *kf, float measurement_noise);

#endif /* MY_KALMAN_FILTER_H */
#include "my_kalman_filter.h"
#include <math.h>
#include <stddef.h>

#include <stdbool.h>

// 卡尔曼滤波器相关函数
void kalman_filter_init(kalman_filter_t *kf, float process_noise, float measurement_noise) {
    if (!kf) return;
    kf->q = process_noise;
    kf->r = measurement_noise;
    kf->x = 0.0f;
    kf->p = 1.0f;
    kf->k = 0.0f;
    kf->initialized = false;
}

float kalman_filter_update(kalman_filter_t *kf, float measurement) {
    if (!kf) return measurement;
    if (!kf->initialized) {
        kf->x = measurement;
        kf->initialized = true;
        return kf->x;
    }
    kf->p = kf->p + kf->q;
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1.0f - kf->k) * kf->p;
    return kf->x;
}

void kalman_filter_reset(kalman_filter_t *kf) {
    if (!kf) return;
    kf->x = 0.0f;
    kf->p = 1.0f;
    kf->initialized = false;
}

void kalman_filter_set_process_noise(kalman_filter_t *kf, float process_noise) {
    if (!kf) return;
    kf->q = process_noise;
}

void kalman_filter_set_measurement_noise(kalman_filter_t *kf, float measurement_noise) {
    if (!kf) return;
    kf->r = measurement_noise;
}
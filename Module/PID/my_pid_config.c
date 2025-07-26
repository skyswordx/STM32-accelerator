#include "my_pid_config.h"
#include <math.h>
#include <stddef.h>

#include <stdbool.h>
#include "my_kalman_filter.h"

// PID 控制器相关函数
void PID_controller_init(PID_controller_t *pid, float output_max, float output_min, float kp, float ki, float kd) {
    if (!pid) return;
    pid->process_variable.measure = 0.0f;
    pid->process_variable.last_measure = 0.0f;
    pid->process_variable.last_output = output_min;
    pid->process_variable.target = 0.0f;
    pid->process_variable.error = 0.0f;
    pid->controller_output = 0.0f;
    pid->CONTROLLER_OUTPUT_MAX = output_max;
    pid->CONTROLLER_OUTPUT_MIN = output_min;
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    kalman_filter_init(&pid->kalman_filter, 0.002f, 0.0005f);
    pid->enable_kalman_filter = true;
    pid->read_sensor = NULL;
    pid->convert_output = NULL;
}

void PID_controller_set_kalmanFilter(PID_controller_t *pid, float process_noise, float measurement_noise) {
    if (!pid) return;
    kalman_filter_set_process_noise(&pid->kalman_filter, process_noise);
    kalman_filter_set_measurement_noise(&pid->kalman_filter, measurement_noise);
}

void PID_controller_enable_kalmanFilter(PID_controller_t *pid, bool enable) {
    if (!pid) return;
    pid->enable_kalman_filter = enable;
    if (!enable) {
        kalman_filter_reset(&pid->kalman_filter);
    }
}

void PID_controller_start(PID_controller_t *pid) {
    if (!pid) return;
    if (pid->process_variable.last_output == 0.0f) {
        pid->process_variable.last_output = pid->CONTROLLER_OUTPUT_MIN;
    }
    pid->controller_output = 0.0f;
    // 获取测量值
    if (pid->read_sensor) {
        float raw_measurement = pid->read_sensor();
        if (pid->enable_kalman_filter) {
            pid->process_variable.measure = kalman_filter_update(&pid->kalman_filter, raw_measurement);
        } else {
            pid->process_variable.measure = raw_measurement;
        }
    }
    // 控制器终止条件
    if ((pid->process_variable.target == 0.0f) && (fabsf(pid->process_variable.measure) < 15.0f)) {
        // 停止输出 pid
        return;
    } else {
        pid->process_variable.error = pid->process_variable.target - pid->process_variable.measure;
        float p_term = pid->kp * pid->process_variable.error;
        float i_term = pid->ki * pid->process_variable.error;
        float d_term = pid->kd * (pid->process_variable.last_measure - pid->process_variable.measure);
        pid->controller_output = pid->process_variable.last_output + p_term + i_term + d_term;
        // float controller_output_limited = fminf(3.0f * from_set_current_mA2voltage_V(pid->process_variable.target), pid->CONTROLLER_OUTPUT_MAX);
        // 这里假设最大输出为 CONTROLLER_OUTPUT_MAX 也可以像上面一样避免调整过大，动态调整限制幅度的值
        float controller_output_limited = pid->CONTROLLER_OUTPUT_MAX; 
        
        if (pid->controller_output > controller_output_limited) {
            i_term -= pid->controller_output - controller_output_limited;
            pid->controller_output = controller_output_limited;
        } else if (pid->controller_output < pid->CONTROLLER_OUTPUT_MIN) {
            i_term += pid->CONTROLLER_OUTPUT_MIN - pid->controller_output;
            pid->controller_output = pid->CONTROLLER_OUTPUT_MIN;
        }
        pid->process_variable.last_measure = pid->process_variable.measure;
        pid->process_variable.last_output = pid->controller_output;
    }
    // 转换控制器输出
    if (pid->convert_output) {
        pid->controller_output = pid->convert_output(pid->controller_output);
    }
}

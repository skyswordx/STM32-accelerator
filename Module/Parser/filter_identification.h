//这里还是490次的方案，NUM_FREQ_POINTS要随着实际情况更改，注意H_measured_cmplx是交错格式
//|H(0)|的w值要尽可能低，|H(end)|的w值要尽可能高，来保证滤波器类型的判断不会出问题
#ifndef INC_FILTER_IDENTIFICATION_H_
#define INC_FILTER_IDENTIFICATION_H_

#include "arm_math.h" // 必须包含的核心库

// 定义仿真参数，最终这些值应由你的测量程序动态填充
#define NUM_FREQ_POINTS  ((50000 - 1000) / 100 + 1) // (f_stop-f_start)/f_step + 1

// 定义滤波器类型枚举
typedef enum {
    FILTER_TYPE_LPF,
    FILTER_TYPE_HPF,
    FILTER_TYPE_BPF,
    FILTER_TYPE_BSF,
    FILTER_TYPE_UNKNOWN
} FilterType;

// 输出结果结构体，存放辨识出的连续系统 H(s) 的系数
// H(s) = (b2*s^2 + b1*s + b0) / (s^2 + a1*s + a0)
typedef struct {
    float32_t b0, b1, b2; // 分子系数
    float32_t a0, a1;     // 分母系数 (a2固定为1)
    FilterType identified_type;
} ContinuousTransferFunction;

// 函数原型
void identify_filter(
    ContinuousTransferFunction* result,
    const float32_t* w_rad,           // 角频率数组 (rad/s)
    const float32_t* H_measured_cmplx // 测量的复数响应 (交错格式 [R,I,R,I...])
);

#endif /* INC_FILTER_IDENTIFICATION_H_ */
#include "my_timer_config.h"
extern TIM_HandleTypeDef htim3;  /* 实际使用TIM3作为ADC触发源和时间戳基准 */
extern TIM_HandleTypeDef htim4;  /* 实际使用TIM4作为DAC触发源和时间戳基准 */

/**
 * @brief 手动设置采样率（直接设置寄存器值）
 * @warning 注意一定要在 timer stop 后使用
 * @param htimer: 定时器句柄指针
 * @param prescaler: 预分频器寄存器值（实际分频 = prescaler + 1）
 * @param arr: 自动重载寄存器值（实际周期 = arr + 1）
 * @retval None
 */
void switch_timer_sampleRate_Manual(TIM_HandleTypeDef* htimer, uint32_t prescaler, uint32_t arr) {

    // 直接设置寄存器值（调用者需要确保参数已经是正确的寄存器值）
    htimer->Instance->PSC = prescaler;  // 实际分频 = prescaler + 1
    htimer->Instance->ARR = arr;        // 实际周期 = arr + 1
    htimer->Instance->EGR = 0x01;       // TIM_EGR_UG - 触发更新事件
}

HAL_StatusTypeDef switch_timer_sampleRate_Auto(TIM_HandleTypeDef* htimer, uint32_t desired_sample_rate) {

    const uint32_t tim_clk = 200000000; // 假设定时器时钟为200MHz
    
    // 参数验证
    if (htimer == NULL || desired_sample_rate == 0) {
        return HAL_ERROR;
    }

    uint32_t total_divisor = tim_clk / desired_sample_rate;
    if (total_divisor < 1) {
        return HAL_ERROR; // 频率太高，无法实现
    }

    // 预分频系数从小开始枚举，计算 ARR
    // 只考虑实际频率与预期频率小于 10Hz 的数值
    
    uint32_t best_prescaler = 0;
    uint32_t best_arr = 0;
    uint32_t min_error = 0xFFFFFFFF;
    uint8_t found = 0;
    
    // 遍历预分频器值：从1开始，优先选择小的预分频器以获得更好的精度
    for (uint32_t psc = 1; psc <= 65536 && psc <= total_divisor; psc++) {
        // 计算对应的ARR值（寄存器值 = 实际值 - 1）
        uint32_t arr_actual = total_divisor / psc;
        
        // 检查ARR是否在有效范围内（实际值应该在2-65536之间）
        if (arr_actual >= 2 && arr_actual <= 65536) {
            uint32_t arr_reg = arr_actual - 1; // ARR寄存器值
            
            // 计算实际频率
            uint32_t actual_freq = tim_clk / (psc * arr_actual);
            
            // 计算误差
            uint32_t error = (actual_freq > desired_sample_rate) ? 
                            (actual_freq - desired_sample_rate) : 
                            (desired_sample_rate - actual_freq);
            
            // 如果误差小于10Hz且比之前的更好
            if (error < 10 && error < min_error) {
                min_error = error;
                best_prescaler = psc - 1;  // PSC寄存器值 = 实际分频 - 1
                best_arr = arr_reg;        // ARR寄存器值（已减1）
                found = 1;
                
                // 如果找到完美匹配，直接退出
                if (error == 0) {
                    break;
                }
            }
        }
    }
    
    // 如果找到合适的参数，设置定时器并返回成功
    if (found) {
        htimer->Instance->PSC = best_prescaler;  // PSC寄存器值
        htimer->Instance->ARR = best_arr;        // ARR寄存器值
        htimer->Instance->EGR = 0x01;            // TIM_EGR_UG - 触发更新事件
        return HAL_OK; // 成功设置采样率
    } else {
        // 没有找到合适的参数组合
        // 可能的原因：
        // 1. desired_sample_rate太高，无法实现
        // 2. desired_sample_rate太低，计算出的ARR超出范围
        // 3. 所有可能组合的误差都>10Hz
        return HAL_ERROR;
    }
}
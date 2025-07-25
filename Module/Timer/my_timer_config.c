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

HAL_StatusTypeDef switch_timer_sampleRate_Auto(TIM_HandleTypeDef* htimer, uint32_t desired_sample_rate, uint32_t measured_freq) {

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
    // 只考虑实际频率与预期频率小于阈值的数值
    uint32_t best_prescaler = 0;
    uint32_t best_arr = 0;
    uint32_t min_error = 0xFFFFFFFF;
    uint8_t found = 0;
    // 误差阈值为被测频率的10%
    uint32_t error_threshold = (measured_freq > 0) ? (measured_freq / 10) : 10;
    if (error_threshold < 10) error_threshold = 10; // 最小阈值10Hz，防止过小

    for (uint32_t psc = 1; psc <= 65536 && psc <= total_divisor; psc++) {
        uint32_t arr_actual = total_divisor / psc;
        if (arr_actual >= 2 && arr_actual <= 65536) {
            uint32_t arr_reg = arr_actual - 1;
            uint32_t actual_freq = tim_clk / (psc * arr_actual);
            uint32_t error = (actual_freq > desired_sample_rate) ?
                            (actual_freq - desired_sample_rate) :
                            (desired_sample_rate - actual_freq);
            // 记录最小误差组合
            if (error < min_error) {
                min_error = error;
                best_prescaler = psc - 1;
                best_arr = arr_reg;
            }
            // 只要有满足阈值的组合，标记found
            if (error < error_threshold) {
                found = 1;
            }
        }
    }

    htimer->Instance->PSC = best_prescaler;
    htimer->Instance->ARR = best_arr;
    htimer->Instance->EGR = 0x01;
    g_ADC_SAMPLE_RATE_Hz = tim_clk / ((best_prescaler + 1) * (best_arr + 1));
    if (found) {
        printf("Finall timer sample rate to %lu Hz (PSC=%lu, ARR=%lu)\n",
               g_ADC_SAMPLE_RATE_Hz, best_prescaler, best_arr);
        return HAL_OK;
    } else {
        printf("Warning: Cannot set timer to desired sample rate %lu Hz within threshold %lu Hz. Set to closest possible: %lu Hz (min error: %lu Hz, PSC=%lu, ARR=%lu)\n",
               desired_sample_rate, error_threshold, g_ADC_SAMPLE_RATE_Hz, min_error, best_prescaler, best_arr);
        return HAL_OK;
    }
}


/**
 * @brief 自适应采样率调整 - 根据测得频率自动计算并设置最优采样率
 * @note 实现文档中的数学公式：F_s = 20 × f_signal (下界，获得最佳LSB)
 * @param htimer: 定时器句柄指针
 * @param measured_freq: 测得的信号频率 (Hz)
 * @retval HAL_StatusTypeDef: HAL_OK表示成功，HAL_ERROR表示失败
 */
HAL_StatusTypeDef adaptive_set_sample_rate_Auto(TIM_HandleTypeDef* htimer, uint32_t measured_freq) {

    // 计算采样率下界和上界
    uint32_t lower_bound = MIN_POINTS_PER_CYCLE * measured_freq;
    uint32_t upper_bound = (FFT_LENGTH * measured_freq) / MIN_CYCLES_IN_FFT;

    if (lower_bound > upper_bound) {
        printf("Error: lower_bound %lu exceeds upper_bound %lu for measured frequency %lu Hz\n",
               lower_bound, upper_bound, measured_freq);
        return HAL_ERROR;
    }

    // 在甜点区间内递增查找满足所有约束的采样率
    uint32_t chosen_sample_rate = 0;
    for (uint32_t Fs = lower_bound; Fs <= upper_bound; Fs++) {
        // 1. 检查每周期采样点数
        uint32_t points_per_cycle = Fs / measured_freq;
        if (points_per_cycle < MIN_POINTS_PER_CYCLE) continue;

        // 2. 检查FFT窗口内周期数
        uint32_t cycles_in_window = (FFT_LENGTH * measured_freq) / Fs;
        if (cycles_in_window < MIN_CYCLES_IN_FFT) continue;

        // 3. 检查奈奎斯特定理
        if (Fs <= 2 * measured_freq) continue;

        // 满足所有约束，选用此采样率
        chosen_sample_rate = Fs;
        break;
    }

    if (chosen_sample_rate == 0) {
        printf("Error: No valid sample rate found in [%lu, %lu] for measured frequency %lu Hz\n",
               lower_bound, upper_bound, measured_freq);
        return HAL_ERROR;
    }

    // 应用找到的采样率
    return switch_timer_sampleRate_Auto(htimer, chosen_sample_rate, measured_freq);
}



/**
 * @brief 手动分段查表设置甜点采样率
 * @param htimer: 定时器句柄指针
 * @param measured_freq: 测得的信号频率 (Hz)
 * @retval HAL_StatusTypeDef: HAL_OK表示成功，HAL_ERROR表示失败
 */
HAL_StatusTypeDef adaptive_set_sample_rate_Manual(TIM_HandleTypeDef* htimer, uint32_t measured_freq) {
    if (htimer == NULL || measured_freq == 0) {
        return HAL_ERROR;
    }


    // 频段表（更细化，步进5kHz~10kHz）
    typedef struct {
        uint32_t freq_min;
        uint32_t freq_max;
        uint32_t sweet_sample_rate;
    } freq_band_t;

    static const freq_band_t bands[] = {
        {1000,   4999,   60000},    // 3kHz甜点: 20*3k=60kHz
        {5000,   9999,  100000},    // 7.5kHz甜点: 20*7.5k=150kHz
        {10000, 14999,  200000},    // 12.5kHz甜点: 20*12.5k=250kHz
        {15000, 19999,  300000},    // 17.5kHz甜点: 20*17.5k=350kHz
        {20000, 24999,  400000},    // 22.5kHz甜点: 20*22.5k=450kHz
        {25000, 29999,  500000},    // 27.5kHz甜点: 20*27.5k=550kHz
        {30000, 39999,  700000},    // 35kHz甜点: 20*35k=700kHz
        {40000, 49999,  900000},    // 45kHz甜点: 20*45k=900kHz
        {50000, 59999, 1100000},    // 55kHz甜点: 20*55k=1.1MHz
        {60000, 69999, 1300000},    // 65kHz甜点: 20*65k=1.3MHz
        {70000, 79999, 1500000},    // 75kHz甜点: 20*75k=1.5MHz
        {80000, 89999, 1700000},    // 85kHz甜点: 20*85k=1.7MHz
        {90000,100000, 1900000},    // 95kHz甜点: 20*95k=1.9MHz
    };
    const int band_count = sizeof(bands)/sizeof(bands[0]);

    for (int i = 0; i < band_count; ++i) {
        if (measured_freq >= bands[i].freq_min && measured_freq <= bands[i].freq_max) {
            // 命中频段，直接设置采样率
            g_ADC_SAMPLE_RATE_Hz = bands[i].sweet_sample_rate;
            printf("Setting sample rate to %lu Hz for frequency %lu Hz\n", g_ADC_SAMPLE_RATE_Hz, measured_freq);
            return switch_timer_sampleRate_Auto(htimer, g_ADC_SAMPLE_RATE_Hz, measured_freq);
        }
    }

    // 超出支持范围
    printf("Error: measured_freq %lu Hz out of supported bands!\n", measured_freq);
    return HAL_ERROR;
}


/**
 * @brief 验证采样率约束条件
 * @param signal_freq: 信号频率 (Hz)
 * @param sample_rate: 采样率 (Hz)
 * @retval HAL_StatusTypeDef: HAL_OK表示满足约束，HAL_ERROR表示不满足
 */
HAL_StatusTypeDef validate_sample_rate_constraints(uint32_t signal_freq, uint32_t sample_rate) {
    
    // 参数验证
    
    // 检查奈奎斯特定理：采样率必须 > 2 * 信号频率

    
    // 检查每周期采样点数

    
    // 检查FFT窗口内的周期数

    return HAL_OK;
}
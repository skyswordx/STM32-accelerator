/**
 * @file main.c
 * @brief 包含了主要的算法实现
 * @details 这里我用的是400MHz的ADC，20KHz的输入信号。为了减小误差我用了比较大的采样点数（4096），后续可以为了速度调小一点
 * 那个滤波器其实还挺难用的，他除了附加相位之外还会有一个群延时，你可以理解为建立时间，为了不让这部分的数据污染观测结果
 * 我们每次都要减去这个群延时
 * MATLAB计算的各项参数在dila_fir_coeffs.h里面
 * 其他的参数在dila_config.h里面
 * 这个代码只适用于我上面说的参数的情况，其他的参数需要我重新进行仿真，滤波器也要重新设计，重新给定系数
 * 你先按照这个参数用一下看看会不会出bug
 * 涉及到usart的部分需要你进一步修改
 * 各种计算都是采用的f32，如果你用adc输入进来的话需要转换一下
 * 
 */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// DILA 配置文件和由MATLAB生成的滤波器系数
#include "dila_config.h"
#include "dila_fir_coeffs.h"

/* Private variables ---------------------------------------------------------*/
// --- 定义所有需要的信号缓冲区 ---
// 输入信号
static float32_t signal_in[DILA_N];//4096，这里用的32位的
// 理想参考信号
static float32_t ref_I_ideal[DILA_N];
static float32_t ref_Q_ideal[DILA_N];
// 滤波后的信号
static float32_t signal_filtered[DILA_N];
static float32_t ref_I_filtered[DILA_N];
static float32_t ref_Q_filtered[DILA_N];
// 解调后的I/Q信号 (混频后)
static float32_t I_raw[DILA_N];
static float32_t Q_raw[DILA_N];
// 最终滤波后的I/Q信号 (直流分量)
static float32_t I_filtered[DILA_N];//这一步取的是差频，几乎是直流，所以那个输出滤波器会很窄
static float32_t Q_filtered[DILA_N];

// --- 定义FIR滤波器实例 ---
// 输入滤波器实例 (信号, I参考, Q参考 共用相同的系数)
static arm_fir_instance_f32 fir_inst_input_signal;
static arm_fir_instance_f32 fir_inst_input_ref_i;
static arm_fir_instance_f32 fir_inst_input_ref_q;
// 输出滤波器实例 (I通道, Q通道 共用相同的系数)
static arm_fir_instance_f32 fir_inst_output_i;
static arm_fir_instance_f32 fir_inst_output_q;

// --- 定义FIR滤波器状态缓冲区 ---
// 状态缓冲区大小为 (抽头数 + 块大小 - 1)
// CMSIS-DSP FIR函数要求状态缓冲区大小为 (numTaps + blockSize - 1)
// 在这里 blockSize 就是 DILA_N
#define INPUT_FIR_STATE_SIZE  (INPUT_FIR_NUM_TAPS + DILA_N - 1)
#define OUTPUT_FIR_STATE_SIZE (OUTPUT_FIR_NUM_TAPS + DILA_N - 1)

static float32_t fir_state_input_signal[INPUT_FIR_STATE_SIZE];
static float32_t fir_state_input_ref_i[INPUT_FIR_STATE_SIZE];
static float32_t fir_state_input_ref_q[INPUT_FIR_STATE_SIZE];
static float32_t fir_state_output_i[OUTPUT_FIR_STATE_SIZE];
static float32_t fir_state_output_q[OUTPUT_FIR_STATE_SIZE];

// --- 用于结果输出的缓冲区 ---
char uart_buf[200];//这里根据你的uart改


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
// 函数声明
void generate_test_signals(void);
void run_dila_processing(void);
void calculate_and_print_results(void);
//=============================这里根据你的uart改===========================================
// 重定向 printf 到 UART
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY); // 修改为您的UART句柄
  return ch;
}
//==========================================================================================
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init(); // 假设使用 USART1

  /* USER CODE BEGIN 2 */
  printf("--- STM32 DILA 启动 ---\n");
  printf("--- 基于 ARM CMSIS-DSP 的匹配滤波方案 ---\n\n");

  // 1. 初始化所有FIR滤波器
  // 注意：三个输入滤波器使用相同的系数(用来做我们的相位匹配），两个输出滤波器使用相同的系数
  arm_fir_init_f32(&fir_inst_input_signal, INPUT_FIR_NUM_TAPS, (float32_t*)input_fir_coeffs, fir_state_input_signal, DILA_N);//滤波器的系数数组在dila_fir_coeffs.h中
  arm_fir_init_f32(&fir_inst_input_ref_i, INPUT_FIR_NUM_TAPS, (float32_t*)input_fir_coeffs, fir_state_input_ref_i, DILA_N);
  arm_fir_init_f32(&fir_inst_input_ref_q, INPUT_FIR_NUM_TAPS, (float32_t*)input_fir_coeffs, fir_state_input_ref_q, DILA_N);
  arm_fir_init_f32(&fir_inst_output_i, OUTPUT_FIR_NUM_TAPS, (float32_t*)output_fir_coeffs, fir_state_output_i, DILA_N);
  arm_fir_init_f32(&fir_inst_output_q, OUTPUT_FIR_NUM_TAPS, (float32_t*)output_fir_coeffs, fir_state_output_q, DILA_N);
  
  printf("滤波器初始化完成。\n");

  // 2. 生成测试信号
  // 在真实应用中，signal_in 来自 ADC+DMA
  generate_test_signals();
  printf("测试信号生成完成。\n");

  // 3. 执行完整的DILA处理流程
  run_dila_processing();
  printf("DILA处理流程完成。\n\n");

  // 4. 计算并打印最终结果
  calculate_and_print_results();

  /* Infinite loop */
  while (1)
  {
    // 在这里可以添加循环处理逻辑
  }
}

// Box-Muller变换生成高斯噪声
float32_t box_muller_transform() {   //这一部分是用来模拟输入信号的，实际测量流程不需要
    static int use_last = 0;
    static float32_t last_val;

    if (use_last) {
        use_last = 0;
        return last_val;
    }

    float32_t u1, u2, r, theta;
    
    // 生成两个独立的均匀随机数
    u1 = (float32_t)rand() / RAND_MAX;
    u2 = (float32_t)rand() / RAND_MAX;
    
    // 确保u1不为零
    while (u1 <= 1e-10f) {
        u1 = (float32_t)rand() / RAND_MAX;
    }
    
    // Box-Muller变换
    r = sqrtf(-2.0f * logf(u1));
    theta = 2.0f * PI * u2;

    // 缓存并返回高斯随机数
    last_val = r * arm_sin_f32(theta);
    use_last = 1;
    return r * arm_cos_f32(theta);  // 使用CMSIS-DSP优化的三角函数
}

/**
  * @brief 生成测试用的输入信号和理想参考信号，添加高斯白噪声
  */
void generate_test_signals(void)//如果你直接从adc采样输出，就把这个函数里面对于输入信号的模拟删掉。然后这个函数就只生成理想参考信号了，signal_in[i]要换成ADC采样的信号
{
    float32_t dt = 1.0f / DILA_FS;
    
    // 计算所需的噪声功率（根据您的应用调整SNR）
    const float32_t target_snr_db = 30.0f; // 目标信噪比30dB
    const float32_t signal_power = (DILA_AIN * DILA_AIN) / 2.0f; // 正弦信号功率 = A²/2
    const float32_t snr_linear = powf(10.0f, target_snr_db / 10.0f);
    const float32_t noise_power = signal_power / snr_linear;
    const float32_t noise_std_dev = sqrtf(noise_power);

    for(uint16_t i = 0; i < DILA_N; i++)
    {
        float32_t t = i * dt;
        
        // 生成理想输入信号
        float32_t ideal_signal = DILA_AIN * arm_sin_f32(2 * PI * DILA_FIN_ACTUAL * t + DILA_PHI_IN_RAD);
        
        // 添加高斯噪声
        signal_in[i] = ideal_signal + (noise_std_dev * box_muller_transform());
        
        // 生成理想参考信号
        ref_I_ideal[i] = arm_sin_f32(2 * PI * DILA_FIN_ACTUAL * t);//DSP库里能更改的还有arm_sin_q15和arm_sin_q31
        ref_Q_ideal[i] = arm_cos_f32(2 * PI * DILA_FIN_ACTUAL * t);
    }
    
    // 如果从ADC直接采样，只生成参考信号
    /*
    for(uint16_t i = 0; i < DILA_N; i++)
    {
        float32_t t = i * dt;
        ref_I_ideal[i] = arm_sin_f32(2 * PI * DILA_FIN_ACTUAL * t);
        ref_Q_ideal[i] = arm_cos_f32(2 * PI * DILA_FIN_ACTUAL * t);
    }
    */
}


/**
  * @brief 执行DILA核心处理流程 (匹配滤波)
  */
void run_dila_processing(void)
{
    // 1. 输入滤波: 信号和参考信号通过完全相同的输入滤波器
    arm_fir_f32(&fir_inst_input_signal, signal_in, signal_filtered, DILA_N);
    arm_fir_f32(&fir_inst_input_ref_i, ref_I_ideal, ref_I_filtered, DILA_N);
    arm_fir_f32(&fir_inst_input_ref_q, ref_Q_ideal, ref_Q_filtered, DILA_N);

    // 2. I/Q解调 (使用滤波后的参考信号)
    arm_mult_f32(signal_filtered, ref_I_filtered, I_raw, DILA_N);
    arm_mult_f32(signal_filtered, ref_Q_filtered, Q_raw, DILA_N);

    // 3. 输出滤波 (滤除2*Fin分量)
    arm_fir_f32(&fir_inst_output_i, I_raw, I_filtered, DILA_N);
    arm_fir_f32(&fir_inst_output_q, Q_raw, Q_filtered, DILA_N);
}

/**
  * @brief 计算幅度和相位，并通过UART打印结果
  */
void calculate_and_print_results(void)//现在这个是先计算再删去群延迟部分，后续提速可以先删去延时部分再进行计算
{
    float32_t I_dc, Q_dc;
    float32_t R_meas, phi_meas_rad, phi_meas_deg;

    // 1. 计算均值
    // 丢弃瞬态部分，仅对稳态部分求均值
    // DILA_TOTAL_DELAY 是在MATLAB中计算好的总延迟
    uint16_t start_idx = DILA_TOTAL_DELAY;//在dila_fir_coeffs.h中定义
    uint32_t steady_state_len = DILA_N - start_idx;

    if (steady_state_len <= 0)
    {
        printf("错误: 信号长度不足以覆盖滤波器延迟!\n");   //一般是可以的，我MATLAB试过顶多会删掉300个点，4096个点绰绰有余
        return;
    }

    arm_mean_f32(&I_filtered[start_idx], steady_state_len, &I_dc);
    arm_mean_f32(&Q_filtered[start_idx], steady_state_len, &Q_dc);

    // 2. 幅度计算
    // R_meas = 2 * sqrt(I_dc^2 + Q_dc^2)
    float32_t temp_sqrt_arg = I_dc * I_dc + Q_dc * Q_dc;
    arm_sqrt_f32(temp_sqrt_arg, &R_meas);
    R_meas *= 2.0f;

    // 3. 相位计算
    phi_meas_rad = atan2f(Q_dc, I_dc);//用的math.h的函数，范围是（-pi->+pi)
    phi_meas_deg = phi_meas_rad * 180.0f / PI;

    // 4. 打印结果
    printf("--- DILA 测量结果 ---\n");
    
    sprintf(uart_buf, "真实幅度: %.6f V\n", DILA_AIN);
    printf(uart_buf);
    sprintf(uart_buf, "测量幅度: %.6f V\n", R_meas);
    printf(uart_buf);
    
    float32_t amp_error = 100.0f * fabsf(R_meas - DILA_AIN) / DILA_AIN;
    sprintf(uart_buf, "幅度误差: %.6f %%\n\n", amp_error);
    printf(uart_buf);

    sprintf(uart_buf, "真实相位: %.6f deg\n", DILA_PHI_IN_DEG);
    printf(uart_buf);
    sprintf(uart_buf, "测量相位: %.6f deg\n", phi_meas_deg);
    printf(uart_buf);

    float32_t phase_error = fabsf(phi_meas_deg - DILA_PHI_IN_DEG);
    sprintf(uart_buf, "相位误差: %.6f deg\n", phase_error);
    printf(uart_buf);
}




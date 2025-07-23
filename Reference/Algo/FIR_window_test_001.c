//大体上ai写的，我又顺了一遍，基本上api调用都改过来了，这个和之前MATLAB的仿真代码的处理逻辑是相同的
//正弦波->加噪声->FIR->HANNING->RFFT，如果没问题就把前面的信号删掉了
//可能数组大小有一点问题，你先试一试。我还没有改成4096的版本
//那个FIR的你先用着，我一会看看MATLAB的这种信号处理的内置函数怎么用
//有bug找我

#include "main.h"
#include "arm_math.h" // 核心：包含CMSIS-DSP库
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
// --- 仿真参数 ---
#define F_SIGNAL    500250.0f   // 信号频率 (500.25 kHz)
#define FS          4000000.0f  // 采样频率 (4 MHz)
#define T_SIM       0.001f      // 仿真时长 (2 ms)
#define N_SAMPLES   ((int)(FS * T_SIM)) // 采样点数 (4000)
#define AMPLITUDE   1.0f        // 信号幅度
#define SNR_DB      10.0f       // 信噪比

// --- 滤波器参数 ---
#define FIR_ORDER   100
#define NUM_TAPS    (FIR_ORDER + 1)

// --- FFT参数 ---
#define FFT_SIZE    N_SAMPLES // FFT点数与采样点数相同

/* Private variables ---------------------------------------------------------*/
// --- 信号缓冲区 ---
// 使用 'static' 关键字将大数组分配在.bss 或.data 段，防止栈溢出
static float32_t ideal_signal[N_SAMPLES];
static float32_t noisy_signal[N_SAMPLES];
static float32_t filtered_signal[N_SAMPLES];
static float32_t windowed_signal[N_SAMPLES];

// --- 汉宁窗缓冲区 ---
static float32_t hanning_window[N_SAMPLES];

// --- FFT相关缓冲区 ---
// FFT输出缓冲区，实数FFT的输出是复数，但以特殊格式打包，大小与输入相同
static float32_t fft_output[N_SAMPLES]; 
// 幅度谱缓冲区，大小为FFT点数的一半
static float32_t magnitude_spectrum[[FFT_SIZE/2]];//可能要加一，取决于样本大小

// --- CMSIS-DSP 实例结构体 ---
static arm_fir_instance_f32 fir_instance;
static arm_rfft_fast_instance_f32 rfft_instance;

// --- FIR滤波器状态缓冲区 ---
// CMSIS-DSP FIR需要一个状态缓冲区来存储历史数据
// 大小为 (滤波器阶数 + 块大小 - 1)
static float32_t fir_state[NUM_TAPS + N_SAMPLES - 1];

// --- FIR滤波器系数 (从MATLAB导出，时间反转) ---
// CMSIS-DSP FIR函数要求系数是时间反转的 (用MATLAB的fliplr(b)获得)
const float32_t fir_coeffs_reversed = {
  0.0000000000f, -0.0031576157f, -0.0056705475f, -0.0074901581f,
  -0.0085468292f, -0.0088310242f, -0.0083818436f, -0.0072841644f,
  -0.0056695938f, -0.0036993027f, -0.0015525818f, 0.0005912781f,
  0.0025644302f, 0.0042314529f, 0.0054845810f, 0.0062475204f,
  0.0064764023f, 0.0061693192f, 0.0053644180f, 0.0041418076f,
  0.0026130676f, 0.0009164810f, -0.0007948875f, -0.0023612976f,
  -0.0036878586f, -0.0046882629f, -0.0053069592f, -0.0055170059f,
  -0.0053215027f, -0.0047502518f, -0.0038585663f, -0.0027217865f,
  -0.0014295578f, -0.0000819206f, 0.0012121201f, 0.0023491859f,
  0.0032620430f, 0.0038878918f, 0.0041837692f, 0.0041289330f,
  0.0037317276f, 0.0030288696f, 0.0020813942f, 0.0009660721f,
  -0.0002231598f, -0.0013866425f, -0.0024399757f, -0.0032968521f,
  -0.0038771630f, -0.0041165352f, -0.0039682388f, -0.0034055710f,
  -0.0024213791f, -0.0010259151f, 0.0007538795f, 0.0028758049f,
  0.0053052902f, 0.0079822540f, 0.0108385086f, 0.0137977600f,
  0.0167789459f, 0.0196990967f, 0.0224733353f, 0.0250196457f,
  0.0272645950f, 0.0291385651f, 0.0305817127f, 0.0315485001f,
  0.0320081711f, 0.0319480896f, 0.0313739777f, 0.0303096771f,
  0.0287914276f, 0.0268673897f, 0.0245971680f, 0.0220496655f,
  0.0193028450f, 0.0164411068f, 0.0135518312f, 0.0107194185f,
  0.0080198050f, 0.0055199862f, 0.0032738447f, 0.0013215542f,
  -0.0003117323f, -0.0016161203f, -0.0026068687f, -0.0033011436f,
  -0.0037278533f, -0.0039228201f, -0.0039248466f, -0.0037751794f,
  -0.0035128000f, -0.0031735897f, -0.0027891994f, -0.0023868203f,
  -0.0019888282f, -0.0016128421f, -0.0012711883f, -0.0009718537f,
  -0.0007191181f, -0.0005131834f, -0.0003518164f, -0.0002306104f,
  -0.0001438867f, -0.0000854731f, -0.0000492941f, -0.0000301011f,
};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void generate_sine_wave(float32_t* buffer, uint32_t num_samples);
void add_awgn(float32_t* signal, uint32_t num_samples, float32_t snr_db);
void run_dsp_processing_chain(void);


int main(void)
{
  /* MCU配置 */
  HAL_Init();
  SystemClock_Config();//这个换成自己的时钟配置

  /* 运行一次完整的DSP处理链 */
  run_dsp_processing_chain();

  while (1)
  {

  }
}


void run_dsp_processing_chain(void)
{
    // --- 步骤 1: 生成理想正弦波 ---
    generate_sine_wave(ideal_signal, N_SAMPLES);

    // --- 步骤 2: 添加高斯白噪声 ---
    memcpy(noisy_signal, ideal_signal, N_SAMPLES * sizeof(float32_t));//将理想信号数组复制到噪声信号数组
    add_awgn(noisy_signal, N_SAMPLES, SNR_DB);

    // --- 步骤 3: 初始化并应用FIR低通滤波器 ---
    // 初始化FIR滤波器实例
    // 参数: 实例指针, 滤波器阶数, 反转的系数指针, 状态缓冲区指针, 块大小
    arm_fir_init_f32(&fir_instance, NUM_TAPS, (float32_t*)fir_coeffs_reversed, fir_state, N_SAMPLES);//系数在上面有定义，后续可以用MATLAB改一下
    
    // 应用FIR滤波器
    // 参数: 实例指针, 输入数据指针, 输出数据指针, 块大小
    arm_fir_f32(&fir_instance, noisy_signal, filtered_signal, N_SAMPLES);

    // --- 步骤 4: 生成并应用汉宁窗 ---
    // 使用CMSIS-DSP函数生成汉宁窗
    arm_hanning_f32(hanning_window, N_SAMPLES);

    // 应用汉宁窗 (逐点相乘) 
    for(uint32_t n = 0; n < N_SAMPLES; n++) {
        hanning_window[n] = 0.5f - 0.5f * arm_cos_f32(2 * PI * n / (N_SAMPLES - 1));
    }   //DSP库没有窗函数的定义，只能直接算。可能对于固定长度后续可以用SIMD加速
    
    // 应用汉宁窗
    arm_mult_f32(filtered_signal, hanning_window, windowed_signal, N_SAMPLES);

    // --- 步骤 5: 执行FFT并计算幅度谱 ---
    // 初始化实数FFT实例
    arm_rfft_fast_init_f32(&rfft_instance, FFT_SIZE);//这里指定了长度，每次计算的时候内部会重新计算旋转因子，库函数中对于特定长度的FFT有特定的优化，可以考虑换成arm_rfft_4096_fast_init_f32

    // 执行FFT。输入是实数，输出是打包的复数格式
    // 参数: 实例指针, 输入数据指针, 输出数据指针, FFT方向标志(0=正向, 1=反向)
    arm_rfft_fast_f32(&rfft_instance, windowed_signal, fft_output, 0);

    // 计算复数FFT输出的幅度
    // 参数: 输入(打包的复数), 输出(幅度), FFT大小
    arm_cmplx_mag_f32(fft_output, magnitude_spectrum, FFT_SIZE / 2);//对于N点实数FFT输出，由于其共轭对称，取numSamples = N/2

    // --- 处理完成 ---
    // 此时, 'magnitude_spectrum' 数组中包含了最终的单边幅度谱。
    // 你可以通过调试器观察这个数组，或者通过UART/SWO将其发送到PC进行绘图验证。
    // 注意：为了得到与MATLAB相同的物理幅度，还需要进行归一化。
    // 例如，除以FFT_SIZE，并对除直流和奈奎斯特频率外的所有分量乘以2。
}


/**
  * @brief 生成正弦波
  */
void generate_sine_wave(float32_t* buffer, uint32_t num_samples) {
    for (uint32_t i = 0; i < num_samples; ++i) {
        float32_t t = (float32_t)i / FS;
        buffer[i] = AMPLITUDE * arm_sin_f32(2.0f * PI * F_SIGNAL * t);
    }
}

/**
  * @brief 使用Box-Muller变换生成高斯噪声并添加到信号中
  */
float32_t box_muller_transform() {
    static int use_last = 0;
    static float32_t last_val;

    if (use_last) {
        use_last = 0;
        return last_val;
    }

    float32_t u1, u2, r, theta;
    do {
        u1 = (float32_t)rand() / RAND_MAX;
        u2 = (float32_t)rand() / RAND_MAX;
    } while (u1 == 0.0f);

    r = sqrtf(-2.0f * logf(u1));
    theta = 2.0f * PI * u2;

    last_val = r * arm_sin_f32(theta);
    use_last = 1;
    return r * arm_cos_f32(theta);
}

void add_awgn(float32_t* signal, uint32_t num_samples, float32_t snr_db) {
    float32_t signal_power = 0.0f;
    arm_power_f32(signal, num_samples, &signal_power);
    signal_power /= num_samples;

    float32_t snr_linear = powf(10.0f, snr_db / 10.0f);
    float32_t noise_power = signal_power / snr_linear;
    float32_t noise_std_dev = sqrtf(noise_power);

    for (uint32_t i = 0; i < num_samples; ++i) {
        float32_t noise = noise_std_dev * box_muller_transform();
        signal[i] += noise;
    }
}

/**
  * @brief 系统时钟配置
  */
void SystemClock_Config(void)
{
  //这里需要改成自己的时钟配置
}


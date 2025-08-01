#include "my_dac_task.h"
#include "my_parameter_config.h"
#include "filter_identification.h"

// 定义ADC样本大小（与ADC任务保持一致）
#define ADC_SAMPLE_SIZE (4096)

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim4;

// 引用外部定义的DAC输出模式变量
extern dac_output_mode_t g_dac_output_mode;

// 全局变量定义
DDS_Generator_t g_dds_generator;

// 内部变量
static uint16_t g_dac_imitate_buffer[ADC_SAMPLE_SIZE];
static uint32_t g_dac_imitate_length = 0;

// 电压转DAC值的宏定义
#define VOLTAGE_TO_DAC_VALUE(voltage) ((uint16_t)((voltage) * 4095.0f / 3.3f))

void update_dac_waveform_by_parameters(void)
{
    // 根据当前配置的波形类型设置DDS波形
    switch (g_desired_DAC_output_waveform) {
        case 0: // 正弦波
            DDS_SetWaveform(&g_dds_generator, g_dac_sine_25, 25);
            break;
        case 1: // 方波
            DDS_SetWaveform(&g_dds_generator, g_dac_square_25, 25);
            break;
        case 2: // 三角波
            DDS_SetWaveform(&g_dds_generator, g_dac_triangle_25, 25);
            break;
        default:
            DDS_SetWaveform(&g_dds_generator, g_dac_sine_25, 25);
            break;
    }
    
    // 设置频率
    DDS_SetFrequency(&g_dds_generator, (float)g_desired_DAC_output_frequency);
    
    // 设置幅度（假设0-1对应0-3.3V）
    DDS_SetAmplitude(&g_dds_generator, g_desired_DAC_single_output_amplitude / 3.3f);
}

void set_dac_imitate_mode(const float32_t* output_waveform, uint32_t length)
{
    // 将浮点波形转换为DAC值并存储
    if (length > ADC_SAMPLE_SIZE) {
        length = ADC_SAMPLE_SIZE;
    }
    
    g_dac_imitate_length = length;
    
    for (uint32_t i = 0; i < length; i++) {
        // 限制电压范围到0-3.3V
        float32_t voltage = output_waveform[i];
        if (voltage < 0.0f) voltage = 0.0f;
        if (voltage > 3.3f) voltage = 3.3f;
        
        g_dac_imitate_buffer[i] = VOLTAGE_TO_DAC_VALUE(voltage);
    }
    
    g_dac_output_mode = DAC_OUTPUT_IMITATE;
}

void StartDACProcessingTask(void *argument) 
{
    // 初始化DDS生成器
    DDS_Init(&g_dds_generator, &hdac1, DAC_CHANNEL_1, &htim4, 1000000); // 1MHz更新频率
    
    // 启动定时器
    HAL_TIM_Base_Start(&htim4);
    
    // 首次生成波形数据
    update_dac_waveform_by_parameters();
    
    // 设置DAC通道2的固定电压输出
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2
    
    // 根据初始模式启动DAC
    switch (g_dac_output_mode) {
        case DAC_OUTPUT_STATIC:
            // 静态模式，只输出固定电压
            break;
            
        case DAC_OUTPUT_WAVEFORM:
            // 波形模式，启动DDS
            DDS_Start(&g_dds_generator);
            break;
            
        case DAC_OUTPUT_IMITATE:
            // 模仿模式，使用自定义波形
            if (g_dac_imitate_length > 0) {
                HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_imitate_buffer, g_dac_imitate_length, DAC_ALIGN_12B_R);
            }
            break;
    }
    
    for(;;)
    {
        // 检查是否需要更新输出模式
        static dac_output_mode_t last_mode = DAC_OUTPUT_STATIC;
        
        if (g_dac_output_mode != last_mode) {
            // 停止当前输出
            HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
            DDS_Stop(&g_dds_generator);
            
            // 根据新模式启动输出
            switch (g_dac_output_mode) {
                case DAC_OUTPUT_STATIC:
                    // 只保持静态输出
                    break;
                    
                case DAC_OUTPUT_WAVEFORM:
                    update_dac_waveform_by_parameters();
                    DDS_Start(&g_dds_generator);
                    break;
                    
                case DAC_OUTPUT_IMITATE:
                    if (g_dac_imitate_length > 0) {
                        HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_imitate_buffer, g_dac_imitate_length, DAC_ALIGN_12B_R);
                    }
                    break;
            }
            
            last_mode = g_dac_output_mode;
        }
        
        osDelay(100); // 延时100毫秒
    }
}

/**
 * @file    AD9554_cubemx_example.c
 * @brief   AD9554驱动库CubeMX使用示例
 * @note    本示例展示如何在CubeMX生成的项目中使用AD9554驱动
 * @author  您的名字
 * @date    2024
 */

#include "main.h"
#include "AD9554.h"

/**
 * @brief   AD9554毫秒延时函数实现
 * @param   ms: 延时时间（毫秒）
 * @note    用户必须实现此函数
 */
void AD9554_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief   AD9554微秒延时函数实现
 * @param   us: 延时时间（微秒）
 * @note    用户必须实现此函数
 */
void AD9554_DelayUs(uint32_t us)
{
    // 简单的微秒延时实现
    uint32_t start = HAL_GetTick();
    while((HAL_GetTick() - start) < ((us + 999) / 1000));
}

/**
 * @brief   AD9554基本功能演示
 * @note    在main函数中调用
 */
void AD9554_BasicDemo(void)
{
    // 1. 初始化AD9554
    AD9554_Init();
    
    // 2. 设置基本参数
    AD9554_SetFrequency(1000000.0);    // 设置频率为1MHz
    AD9554_SetAmplitude(0x3000);       // 设置幅度为75%
    AD9554_SetPhase(0);                // 设置相位为0度
    
    // 3. 等待设置生效
    HAL_Delay(100);
    
    printf("AD9554已初始化，频率：1MHz，幅度：75%%\\n");
}

/**
 * @brief   AD9554频率扫描演示
 * @note    演示如何进行频率扫描
 */
void AD9554_FrequencySweepDemo(void)
{
    double freq_start = 100000.0;     // 起始频率：100kHz
    double freq_end = 10000000.0;     // 结束频率：10MHz
    double freq_step = 100000.0;      // 步进：100kHz
    double current_freq;
    
    printf("开始频率扫描：%0.0f Hz -> %0.0f Hz\\n", freq_start, freq_end);
    
    for(current_freq = freq_start; current_freq <= freq_end; current_freq += freq_step)
    {
        AD9554_SetFrequency(current_freq);
        printf("当前频率：%0.0f Hz\\n", current_freq);
        HAL_Delay(500);  // 停留500ms
    }
    
    printf("频率扫描完成\\n");
}

/**
 * @brief   AD9554幅度调制演示
 * @note    演示如何进行幅度调制
 */
void AD9554_AmplitudeModulationDemo(void)
{
    uint16_t amplitude;
    
    printf("开始幅度调制演示\\n");
    
    // 设置固定频率
    AD9554_SetFrequency(1000000.0);
    
    // 幅度从0%到100%变化
    for(amplitude = 0; amplitude <= 0x3FFF; amplitude += 0x400)
    {
        AD9554_SetAmplitude(amplitude);
        printf("当前幅度：%d/16383 (%0.1f%%)\\n", amplitude, (amplitude * 100.0) / 0x3FFF);
        HAL_Delay(200);
    }
    
    printf("幅度调制演示完成\\n");
}

/**
 * @brief   AD9554相位调制演示
 * @note    演示如何进行相位调制
 */
void AD9554_PhaseModulationDemo(void)
{
    uint16_t phase;
    
    printf("开始相位调制演示\\n");
    
    // 设置固定频率和幅度
    AD9554_SetFrequency(1000000.0);
    AD9554_SetAmplitude(0x3000);
    
    // 相位从0度到360度变化
    for(phase = 0; phase < 0x4000; phase += 0x400)
    {
        AD9554_SetPhase(phase);
        printf("当前相位：%d/16384 (%0.1f度)\\n", phase, (phase * 360.0) / 0x4000);
        HAL_Delay(200);
    }
    
    printf("相位调制演示完成\\n");
}

/**
 * @brief   主函数示例
 * @note    展示如何在main函数中使用AD9554驱动
 */
void AD9554_MainExample(void)
{
    /* 
     * 在实际的main函数中，应该包含以下代码：
     * 
     * int main(void)
     * {
     *     // HAL库初始化
     *     HAL_Init();
     *     
     *     // 系统时钟配置
     *     SystemClock_Config();
     *     
     *     // GPIO初始化（CubeMX自动生成）
     *     MX_GPIO_Init();
     *     
     *     // 其他外设初始化...
     *     
     *     // AD9554演示
     *     AD9554_BasicDemo();
     *     
     *     while (1)
     *     {
     *         // 可以在这里添加其他演示函数
     *         // AD9554_FrequencySweepDemo();
     *         // AD9554_AmplitudeModulationDemo();
     *         // AD9554_PhaseModulationDemo();
     *         
     *         HAL_Delay(1000);
     *     }
     * }
     */
}

/**
 * @brief   AD9554错误处理示例
 * @note    展示如何进行错误处理
 */
void AD9554_ErrorHandlingExample(void)
{
    // 检查GPIO初始化是否正确
    if(AD9554_SPI_CS_PORT == NULL)
    {
        printf("错误：GPIO端口未初始化\\n");
        return;
    }
    
    // 设置频率前检查参数
    double test_freq = 50000000.0;  // 50MHz
    if(test_freq > 25000000.0)  // 假设最大频率为25MHz
    {
        printf("警告：频率 %0.0f Hz 超过建议值\\n", test_freq);
        test_freq = 25000000.0;
    }
    
    AD9554_SetFrequency(test_freq);
    
    // 设置幅度前检查参数
    uint16_t test_amplitude = 0x5000;  // 超过最大值
    if(test_amplitude > 0x3FFF)
    {
        printf("警告：幅度值 0x%04X 超过最大值\\n", test_amplitude);
        test_amplitude = 0x3FFF;
    }
    
    AD9554_SetAmplitude(test_amplitude);
}

/**
 * @brief   AD9554配置模板
 * @note    可以根据实际需求修改此模板
 */
typedef struct {
    double frequency;      // 频率 (Hz)
    uint16_t amplitude;    // 幅度 (0-0x3FFF)
    uint16_t phase;        // 相位 (0-0x3FFF)
} AD9554_Config_t;

/**
 * @brief   使用配置结构体设置AD9554
 * @param   config: 配置结构体指针
 */
void AD9554_SetConfig(const AD9554_Config_t* config)
{
    if(config == NULL)
    {
        printf("错误：配置指针为空\\n");
        return;
    }
    
    printf("设置AD9554配置：\\n");
    printf("  频率：%0.0f Hz\\n", config->frequency);
    printf("  幅度：0x%04X\\n", config->amplitude);
    printf("  相位：0x%04X\\n", config->phase);
    
    AD9554_SetFrequency(config->frequency);
    AD9554_SetAmplitude(config->amplitude);
    AD9554_SetPhase(config->phase);
}

/**
 * @brief   预定义配置示例
 */
void AD9554_PresetConfigExample(void)
{
    // 定义几个预设配置
    AD9554_Config_t config_1mhz = {
        .frequency = 1000000.0,
        .amplitude = 0x3000,
        .phase = 0x0000
    };
    
    AD9554_Config_t config_5mhz = {
        .frequency = 5000000.0,
        .amplitude = 0x2000,
        .phase = 0x1000
    };
    
    AD9554_Config_t config_10mhz = {
        .frequency = 10000000.0,
        .amplitude = 0x3FFF,
        .phase = 0x2000
    };
    
    // 使用预设配置
    printf("使用1MHz配置\\n");
    AD9554_SetConfig(&config_1mhz);
    HAL_Delay(2000);
    
    printf("使用5MHz配置\\n");
    AD9554_SetConfig(&config_5mhz);
    HAL_Delay(2000);
    
    printf("使用10MHz配置\\n");
    AD9554_SetConfig(&config_10mhz);
    HAL_Delay(2000);
}

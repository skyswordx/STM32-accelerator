#include "ADCOutputTask.h"

/* 外部声明 */
extern osMessageQueueId_t ADCQueueHandle;

/* ADC输出任务句柄 */
osThreadId_t ADCOutputTaskHandle;

/* ADC输出任务属性 */
const osThreadAttr_t ADCOutputTask_attributes = {
  .name = "ADCOutputTask",
  .stack_size = ADC_OUTPUT_TASK_STACK_SIZE * 4,
  .priority = (osPriority_t) ADC_OUTPUT_TASK_PRIORITY,
};

/* ADC输出模式控制 */
static adc_output_mode_t g_adc_output_mode = ADC_OUTPUT_MODE_VOFA;

/* VOFA模式配置 */
static uint32_t g_vofa_sample_count = 0;

/* HMI串口屏模式配置 */
static uint32_t g_HMI_sample_count = 0;

/* 静态函数声明 */
static const char* ADC_GetOutputModeString(adc_output_mode_t mode);

/**
 * @brief ADC数据输出任务
 * @param argument 任务参数 (未使用)
 * @retval None
 * 
 * 功能描述：
 * - 从ADC队列接收采样数据
 * - 根据输出模式选择不同的输出格式
 * - 支持调试模式和示波器模式
 */
void ADCOutputTask(void *argument)
{
    adc_system_data_t received_adc_data;
    static uint32_t last_sample_count = 0;
    
    printf("[%s] ADC Output Task Started\r\n", osThreadGetName(osThreadGetId()));
    
    // 检查ADC队列是否正确初始化
    if (ADCQueueHandle == NULL) {
        printf("[%s] FATAL ERROR: ADCQueueHandle is NULL!\r\n", osThreadGetName(osThreadGetId()));
        for(;;) {
            osDelay(1000);
        }
    }
    
    printf("[%s] Ready to receive ADC data from queue\r\n", osThreadGetName(osThreadGetId()));
    
    // 输出当前模式
    const char* mode_str = ADC_GetOutputModeString(g_adc_output_mode);
    printf("[%s] Current output mode: %s\r\n", osThreadGetName(osThreadGetId()), mode_str);
    
    /* Infinite loop */
    for(;;)
    {
        // 从队列接收ADC数据，超时时间设为1秒
        osStatus_t status = osMessageQueueGet(ADCQueueHandle, 
                                              &received_adc_data, 
                                              NULL, 
                                              1000);
        
        if (status == osOK) {
            // 成功接收到数据
            
            // 检查是否为新数据（通过采样计数判断）
            if (received_adc_data.total_sample_count != last_sample_count) {
                last_sample_count = received_adc_data.total_sample_count;
                
                // 根据输出模式选择不同的处理方式
                switch (g_adc_output_mode) {
                    case ADC_OUTPUT_MODE_DEBUG:
                        ADC_ProcessDebugOutput(&received_adc_data);
                        break;
                        
                    case ADC_OUTPUT_MODE_VOFA:
                        ADC_ProcessVofaOutput(&received_adc_data);
                        break;
                        
                    case ADC_OUTPUT_MODE_RAW_DATA:
                        ADC_ProcessRawDataOutput(&received_adc_data);
                        break;
                        
                    case ADC_OUTPUT_MODE_HMI:
                        ADC_ProcessHMIOutput(&received_adc_data);
                        break;
                        
                    default:
                        // 默认使用调试模式
                        ADC_ProcessDebugOutput(&received_adc_data);
                        break;
                }
            }
            
        } else if (status == osErrorTimeout) {
            // 超时：1秒内没有收到数据
            printf("[%s] WARNING: No ADC data received for 1 second\r\n", 
                   osThreadGetName(osThreadGetId()));
            
            // 检查全局数据状态
            extern adc_system_data_t g_adc_system;
            if (!g_adc_system.sampling_active) {
                printf("[%s] WARNING: ADC sampling is not active\r\n", 
                       osThreadGetName(osThreadGetId()));
            }
            
        } else {
            // 其他错误
            printf("[%s] Queue Get Error: %d\r\n", osThreadGetName(osThreadGetId()), status);
            osDelay(100); // 短暂延迟后重试
        }
        
        // 更新最后输出时间
        extern adc_system_data_t g_adc_system;
        g_adc_system.last_output_time = osKernelGetTickCount();
    }
}

/**
 * @brief 处理调试模式输出
 * @param data ADC系统数据指针
 * @retval None
 */
void ADC_ProcessDebugOutput(const adc_system_data_t *data)
{
    // 每100次采样输出一次详细数据
    if ((data->total_sample_count % 100) == 0) {
        printf("\r\n=== ADC Sample #%lu ===\r\n", data->total_sample_count);
        printf("Timestamp: %lu ms\r\n", osKernelGetTickCount());
        
        for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
            const adc_channel_data_t *ch_data = &data->channels[i];
            const char* status_str = ch_data->window_full ? "READY" : "FILLING";
            
            printf("CH%d[%s]: Raw=%5d, Voltage=%6.3fV, Samples=%lu, %s\r\n",
                   i, 
                   ADC_GetChannelName(i),
                   ch_data->raw_average,
                   ch_data->voltage_average,
                   ch_data->sample_count,
                   status_str);
        }
        
        printf("Errors: %d, Active: %s\r\n", 
               data->error_count,
               data->sampling_active ? "YES" : "NO");
        printf("========================\r\n\n");
    }
    
    // 每1000次采样输出系统状态
    if ((data->total_sample_count % 1000) == 0) {
        printf("\r\n=== ADC System Status ===\r\n");
        printf("Total Samples: %lu\r\n", data->total_sample_count);
        printf("Error Count: %d\r\n", data->error_count);
        printf("Error Rate: %.2f%%\r\n", 
               (float)data->error_count / data->total_sample_count * 100.0f);
        printf("Free Heap: %lu bytes\r\n", xPortGetFreeHeapSize());
        printf("System Uptime: %lu seconds\r\n", osKernelGetTickCount() / 1000);
        
        // 检查队列状态
        uint32_t queue_count = osMessageQueueGetCount(ADCQueueHandle);
        uint32_t queue_space = osMessageQueueGetSpace(ADCQueueHandle);
        printf("Queue Status: %lu/%lu (used/total)\r\n", 
               queue_count, queue_count + queue_space);
        printf("=========================\r\n\n");
    }
}

/**
 * @brief 处理VOFA模式输出
 * @param data ADC系统数据指针
 * @retval None
 */
void ADC_ProcessVofaOutput(const adc_system_data_t *data)
{
    // 每次都输出电压数据，格式：channels: ch0,ch1,ch2,ch3,ch4,ch5\n
    printf("channels: ");
    
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
        const adc_channel_data_t *ch_data = &data->channels[i];
        printf("%.6f", ch_data->voltage_average);
        
        // 添加分隔符，最后一个通道后不加逗号
        if (i < ADC_CHANNEL_COUNT - 1) {
            printf(",");
        }
    }
    printf("\n");
    
    // 增加VOFA采样计数
    g_vofa_sample_count++;
    
    // 每达到存储深度时输出统计信息
    if (g_vofa_sample_count >= VOFA_BUFFER_DEPTH) {
        printf("# VOFA buffer full (%d samples), Total: %lu, Errors: %d\n", 
               VOFA_BUFFER_DEPTH, data->total_sample_count, data->error_count);
        g_vofa_sample_count = 0;
    }
}

/**
 * @brief 处理原始数据模式输出
 * @param data ADC系统数据指针
 * @retval None
 */
void ADC_ProcessRawDataOutput(const adc_system_data_t *data)
{
    // 不带前缀直接输出电压数据，格式：ch0,ch1,ch2,ch3,ch4,ch5\n
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
        const adc_channel_data_t *ch_data = &data->channels[i];
        printf("%.6f", ch_data->voltage_average);
        
        // 添加分隔符，最后一个通道后不加逗号
        if (i < ADC_CHANNEL_COUNT - 1) {
            printf(",");
        }
    }
    printf("\n");
    
    // 增加原始数据采样计数
    g_vofa_sample_count++;
    
    // 每达到存储深度时输出统计信息（带#前缀，避免与图片数据混淆）
    if (g_vofa_sample_count >= VOFA_BUFFER_DEPTH) {
        printf("# Raw data buffer full (%d samples), Total: %lu, Errors: %d\n", 
               VOFA_BUFFER_DEPTH, data->total_sample_count, data->error_count);
        g_vofa_sample_count = 0;
    }
}

/**
 * @brief 处理HMI串口屏模式输出
 * @param data ADC系统数据指针
 * @retval None
 */
void ADC_ProcessHMIOutput(const adc_system_data_t *data)
{
    // 检查选择的通道是否有效
    if (HMI_SELECTED_CHANNEL >= ADC_CHANNEL_COUNT) {
        return;
    }
    
    const adc_channel_data_t *ch_data = &data->channels[HMI_SELECTED_CHANNEL];
    
    // 将电压值转换为8位数据 (0-255)
    // 假设电压范围是0-3.3V，映射到0-255
    uint8_t HMI_value = (uint8_t)(ch_data->voltage_average * 255.0f / 3.3f);
    
    // 确保值在有效范围内
    if (HMI_value > 255) {
        HMI_value = 255;
    }
    
    // 按照HMI协议发送数据到曲线控件
    // 格式：add 曲线控件ID.id,通道ID,数据值\xff\xff\xff
    printf("add %s.id,%d,%d\xff\xff\xff", HMI_CURVE_ID, HMI_CHANNEL_ID, HMI_value);
    
    // 增加HMI采样计数
    g_HMI_sample_count++;
    
    // 每100个点输出一次统计信息
    if (g_HMI_sample_count >= 100) {
        printf("# HMI sent 100 points, Channel: %d(%s), Total: %lu, Errors: %d\n", 
               HMI_SELECTED_CHANNEL, ADC_GetChannelName(HMI_SELECTED_CHANNEL), 
               data->total_sample_count, data->error_count);
        g_HMI_sample_count = 0;
    }
}

/**
 * @brief 设置ADC输出模式
 * @param mode 输出模式
 * @retval None
 */
void ADC_SetOutputMode(adc_output_mode_t mode)
{
    if (mode < ADC_OUTPUT_MODE_MAX) {
        g_adc_output_mode = mode;
        g_vofa_sample_count = 0; // 重置VOFA计数
        g_HMI_sample_count = 0;  // 重置HMI计数
        
        const char* mode_str = ADC_GetOutputModeString(mode);
        printf("ADC output mode changed to: %s\n", mode_str);
    }
}

/**
 * @brief 获取当前ADC输出模式
 * @retval adc_output_mode_t 当前输出模式
 */
adc_output_mode_t ADC_GetOutputMode(void)
{
    return g_adc_output_mode;
}

/**
 * @brief 切换到下一个输出模式（用于调试）
 * @retval None
 */
void ADC_SwitchToNextOutputMode(void)
{
    adc_output_mode_t next_mode = (adc_output_mode_t)((g_adc_output_mode + 1) % ADC_OUTPUT_MODE_MAX);
    ADC_SetOutputMode(next_mode);
}

/**
 * @brief 输出当前ADC配置信息
 * @retval None
 */
void ADC_PrintModeInfo(void)
{
    printf("\n=== ADC Output Mode Info ===\n");
    printf("Available modes:\n");
    printf("  0 - DEBUG: Detailed debug output every 100 samples\n");
    printf("  1 - VOFA: Format 'channels: ch0,ch1,ch2,ch3,ch4,ch5\\n'\n");
    printf("  2 - RAW_DATA: Format 'ch0,ch1,ch2,ch3,ch4,ch5\\n'\n");
    printf("  3 - HMI: Send data to HMI display using 'add' command\n");
    printf("Current mode: %d (%s)\n", g_adc_output_mode, ADC_GetOutputModeString(g_adc_output_mode));
    printf("Buffer depth: %d samples\n", VOFA_BUFFER_DEPTH);
    printf("Channel count: %d\n", ADC_CHANNEL_COUNT);
    printf("HMI selected channel: %d (%s)\n", HMI_SELECTED_CHANNEL, ADC_GetChannelName(HMI_SELECTED_CHANNEL));
    printf("HMI curve ID: %s, Channel ID: %d\n", HMI_CURVE_ID, HMI_CHANNEL_ID);
    printf("============================\n\n");
}

/**
 * @brief 获取输出模式字符串
 * @param mode 输出模式
 * @retval const char* 模式字符串
 */
static const char* ADC_GetOutputModeString(adc_output_mode_t mode)
{
    switch (mode) {
        case ADC_OUTPUT_MODE_DEBUG:
            return "DEBUG";
        case ADC_OUTPUT_MODE_VOFA:
            return "VOFA";
        case ADC_OUTPUT_MODE_RAW_DATA:
            return "RAW_DATA";
        case ADC_OUTPUT_MODE_HMI:
            return "HMI";
        default:
            return "UNKNOWN";
    }
}

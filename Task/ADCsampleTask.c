#include "ADCsampleTask.h"

/* 外部声明 */
extern osMessageQueueId_t ADCQueueHandle;

/* ADC任务句柄 */
osThreadId_t AdcSamplingTaskHandle;
osThreadId_t AdcOutputTaskHandle;

/* ADC采样任务属性 */
const osThreadAttr_t AdcSamplingTask_attributes = {
  .name = "AdcSamplingTask",
  .stack_size = ADC_SAMPLING_TASK_STACK_SIZE * 4,  // 转换为字节
  .priority = (osPriority_t) ADC_SAMPLING_TASK_PRIORITY,
};

/* ADC输出任务属性 */
const osThreadAttr_t AdcOutputTask_attributes = {
  .name = "AdcOutputTask",
  .stack_size = ADC_OUTPUT_TASK_STACK_SIZE * 4,    // 转换为字节
  .priority = (osPriority_t) ADC_OUTPUT_TASK_PRIORITY,
};


/* ADC输出模式枚举 */
typedef enum {
    ADC_OUTPUT_MODE_DEBUG = 0,      // 调试模式 - 原有的详细输出
    ADC_OUTPUT_MODE_OSCILLOSCOPE,   // 示波器模式 - 带前缀输出波形数据
    ADC_OUTPUT_MODE_RAW_DATA,       // 原始数据模式 - 不带前缀输出波形数据
    ADC_OUTPUT_MODE_MAX
} adc_output_mode_t;

/* 全局ADC系统数据 */
adc_system_data_t g_adc_system = {0};

/* ADC输出模式控制 */
// static adc_output_mode_t g_adc_output_mode = ADC_OUTPUT_MODE_DEBUG;
static adc_output_mode_t g_adc_output_mode = ADC_OUTPUT_MODE_OSCILLOSCOPE;

/* 示波器模式配置 */
#define OSCILLOSCOPE_BUFFER_DEPTH  1024  // 存储深度
static uint32_t g_oscilloscope_sample_count = 0;


/* ADC通道配置表 */
static const adc_channel_config_t adc_channel_configs[ADC_CHANNEL_COUNT] = {
    {ADC_CHANNEL_3,  "IN3_PA6",  3.3f/65535.0f},   // PA6  -> ADC1_INP3
    {ADC_CHANNEL_4,  "IN4_PC4",  3.3f/65535.0f},   // PC4  -> ADC1_INP4
    {ADC_CHANNEL_5,  "IN5_PB1",  3.3f/65535.0f},   // PB1  -> ADC1_INP5
    {ADC_CHANNEL_7,  "IN7_PA7",  3.3f/65535.0f},   // PA7  -> ADC1_INP7
    {ADC_CHANNEL_8,  "IN8_PC5",  3.3f/65535.0f},   // PC5  -> ADC1_INP8
    {ADC_CHANNEL_9,  "IN9_PB0",  3.3f/65535.0f},   // PB0  -> ADC1_INP9
};

/**
 * @brief ADC多通道采样任务
 * @param argument 任务参数 (未使用)
 * @retval None
 * 
 * 功能描述：
 * - 10ms周期轮询采样6个ADC通道
 * - 每个通道维护10个采样值的滑动窗口
 * - 计算平均值并更新系统数据
 * - 不断刷新队列中的ADC数据
 */
void AdcSamplingTask(void *argument)
{
    /* USER CODE BEGIN AdcSamplingTask */
    TickType_t last_wake_time;
    const TickType_t sampling_period = pdMS_TO_TICKS(ADC_SAMPLE_PERIOD_MS);
    
    printf("[%s] ADC Sampling Task Started\r\n", osThreadGetName(osThreadGetId()));
    printf("[%s] Sampling Period: %d ms, Channels: %d\r\n", 
           osThreadGetName(osThreadGetId()), ADC_SAMPLE_PERIOD_MS, ADC_CHANNEL_COUNT);
    
    // 检查ADC队列是否正确初始化
    if (ADCQueueHandle == NULL) {
        printf("[%s] FATAL ERROR: ADCQueueHandle is NULL! Queue not initialized!\r\n", 
               osThreadGetName(osThreadGetId()));
        // 无限循环等待，防止任务继续运行
        for(;;) {
            osDelay(1000);
        }
    } else {
        uint32_t queue_capacity = osMessageQueueGetCapacity(ADCQueueHandle);
        uint32_t queue_msg_size = osMessageQueueGetMsgSize(ADCQueueHandle);
        printf("[%s] ADC Queue OK - Capacity: %lu, MsgSize: %lu, DataSize: %lu\r\n",
               osThreadGetName(osThreadGetId()), queue_capacity, queue_msg_size, sizeof(g_adc_system));
        
        // 检查消息大小是否匹配
        if (queue_msg_size < sizeof(g_adc_system)) {
            printf("[%s] WARNING: Queue message size (%lu) < data size (%lu)!\r\n",
                   osThreadGetName(osThreadGetId()), queue_msg_size, sizeof(g_adc_system));
        }
    }
      // 初始化ADC通道
    uint8_t initialization_success = 0;
    if (ADC_InitializeChannels() == HAL_OK) {
        printf("[%s] ADC initialization successful\r\n", osThreadGetName(osThreadGetId()));
        initialization_success = 1;
        
        // 初始化系统数据
        memset(&g_adc_system, 0, sizeof(g_adc_system));
        g_adc_system.sampling_active = 1;
        
        // 获取初始时间戳
        last_wake_time = xTaskGetTickCount();
        printf("[%s] Starting ADC sampling loop...\r\n", osThreadGetName(osThreadGetId()));
    } else {
        printf("[%s] ERROR: ADC initialization failed, entering error mode\r\n", osThreadGetName(osThreadGetId()));
    }
    
    /* Infinite loop */
    for(;;)
    {
        if (initialization_success) {
            // 精确的周期性唤醒
            vTaskDelayUntil(&last_wake_time, sampling_period);
            
            // 轮询采样所有通道
            for (uint8_t ch = 0; ch < ADC_CHANNEL_COUNT; ch++) {
                if (ADC_SampleChannel(ch) != HAL_OK) {
                    g_adc_system.error_count++;
                    printf("[%s] ERROR: Channel %s sampling failed\r\n", 
                           osThreadGetName(osThreadGetId()), ADC_GetChannelName(ch));
                }
            }
            
            // 更新系统采样计数
            g_adc_system.total_sample_count++;
            
            // 输出全部ADC采样数值到消息队列
            if (ADCQueueHandle != NULL) {
                uint8_t status = osMessageQueuePut(ADCQueueHandle, 
                                                   &g_adc_system,
                                                   0, 
                                                   0);
                if (status != osOK) {
                    // 只在队列操作失败时输出错误信息
                    printf("[%s] Queue Put Error: %d", osThreadGetName(osThreadGetId()), status);
                    switch (status) {
                        case osErrorParameter:
                            printf(" (Invalid params)");
                            break;
                        case osErrorTimeout:
                            printf(" (Queue full)");
                            break;
                        case osErrorResource:
                            printf(" (Not available)");
                            break;
                        default:
                            printf(" (Unknown)");
                            break;
                    }
                    printf("\r\n");
                    
                    // 每1000次错误输出队列状态
                    static uint32_t error_count = 0;
                    if (++error_count % 1000 == 0) {
                        uint32_t queue_count = osMessageQueueGetCount(ADCQueueHandle);
                        uint32_t queue_space = osMessageQueueGetSpace(ADCQueueHandle);
                        printf("[%s] Queue Status: %lu/%lu, Data Size: %lu\r\n",
                               osThreadGetName(osThreadGetId()), 
                               queue_count, queue_count + queue_space, sizeof(g_adc_system));
                    }
                }
            } else {
                printf("[%s] ERROR: ADCQueueHandle is NULL!\r\n", osThreadGetName(osThreadGetId()));
            }
        } else {
            // 错误模式：定期尝试重新初始化
            printf("[%s] Retrying ADC initialization...\r\n", osThreadGetName(osThreadGetId()));
            if (ADC_InitializeChannels() == HAL_OK) {
                printf("[%s] ADC re-initialization successful\r\n", osThreadGetName(osThreadGetId()));
                initialization_success = 1;
                memset(&g_adc_system, 0, sizeof(g_adc_system));
                g_adc_system.sampling_active = 1;
                last_wake_time = xTaskGetTickCount();
            } else {
                osDelay(5000); // 等待5秒后重试
            }
        }
    }
    /* USER CODE END AdcSamplingTask */
}

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
void AdcOutputTask(void *argument)
{
    /* USER CODE BEGIN AdcOutputTask */
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
    const char* mode_str;
    switch (g_adc_output_mode) {
        case ADC_OUTPUT_MODE_DEBUG:
            mode_str = "DEBUG";
            break;
        case ADC_OUTPUT_MODE_OSCILLOSCOPE:
            mode_str = "OSCILLOSCOPE";
            break;
        case ADC_OUTPUT_MODE_RAW_DATA:
            mode_str = "RAW_DATA";
            break;
        default:
            mode_str = "UNKNOWN";
            break;
    }
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
                        
                    case ADC_OUTPUT_MODE_OSCILLOSCOPE:
                        ADC_ProcessOscilloscopeOutput(&received_adc_data);
                        break;
                        
                    case ADC_OUTPUT_MODE_RAW_DATA:
                        ADC_ProcessRawDataOutput(&received_adc_data);
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
        g_adc_system.last_output_time = osKernelGetTickCount();
    }
    /* USER CODE END AdcOutputTask */
}

/**
 * @brief 初始化ADC通道配置
 * @retval HAL_StatusTypeDef HAL状态
 */
HAL_StatusTypeDef ADC_InitializeChannels(void)
{
    HAL_StatusTypeDef status = HAL_OK;
    
    // 校准ADC
    status = HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    if (status != HAL_OK) {
        printf("[ADC] ERROR: ADC calibration failed\r\n");
        return status;
    }
    
    printf("[ADC] ADC calibration completed successfully\r\n");
    printf("[ADC] Configured channels:\r\n");
    
    // 输出通道配置信息
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
        printf("[ADC]   CH%d: %s (HAL_CH=0x%02lX)\r\n", 
               i, adc_channel_configs[i].name, adc_channel_configs[i].channel);
    }
    
    return HAL_OK;
}

/**
 * @brief 采样指定ADC通道
 * @param channel_index 通道索引 (0-10)
 * @retval HAL_StatusTypeDef HAL状态
 */
HAL_StatusTypeDef ADC_SampleChannel(uint8_t channel_index)
{
    if (channel_index >= ADC_CHANNEL_COUNT) {
        return HAL_ERROR;
    }
    
    ADC_ChannelConfTypeDef sConfig = {0};
    HAL_StatusTypeDef status;
    uint32_t adc_value;
    
    // 配置ADC通道
    sConfig.Channel = adc_channel_configs[channel_index].channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSignedSaturation = DISABLE;
    
    // 配置通道
    status = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    if (status != HAL_OK) {
        return status;
    }
    
    // 启动ADC转换
    status = HAL_ADC_Start(&hadc1);
    if (status != HAL_OK) {
        return status;
    }
    
    // 等待转换完成
    status = HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS);
    if (status != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return status;
    }
    
    // 读取ADC值
    adc_value = HAL_ADC_GetValue(&hadc1);
    
    // 停止ADC
    HAL_ADC_Stop(&hadc1);
    
    // 更新滑动窗口
    ADC_UpdateSlidingWindow(channel_index, (uint16_t)adc_value);
    
    return HAL_OK;
}

/**
 * @brief 更新指定通道的滑动窗口平均值
 * @param channel_index 通道索引
 * @param new_value 新的ADC值
 * @retval None
 */
void ADC_UpdateSlidingWindow(uint8_t channel_index, uint16_t new_value)
{
    if (channel_index >= ADC_CHANNEL_COUNT) {
        return;
    }
    
    adc_channel_data_t *ch_data = &g_adc_system.channels[channel_index];
    
    // 移除旧值 (如果窗口已满)
    if (ch_data->window_full) {
        ch_data->raw_sum -= ch_data->raw_values[ch_data->window_index];
    }
    
    // 添加新值
    ch_data->raw_values[ch_data->window_index] = new_value;
    ch_data->raw_sum += new_value;
    
    // 更新索引
    ch_data->window_index = (ch_data->window_index + 1) % ADC_SAMPLE_WINDOW_SIZE;
    
    // 检查窗口是否已满
    if (!ch_data->window_full && ch_data->window_index == 0) {
        ch_data->window_full = 1;
    }
    
    // 计算平均值
    uint8_t sample_count = ch_data->window_full ? ADC_SAMPLE_WINDOW_SIZE : ch_data->window_index;
    if (sample_count > 0) {
        ch_data->raw_average = ch_data->raw_sum / sample_count;
        ch_data->voltage_average = ADC_CalculateVoltage(ch_data->raw_average);
    }
    
    // 更新采样计数
    ch_data->sample_count++;
}

/**
 * @brief 将ADC原始值转换为电压值
 * @param raw_value ADC原始值
 * @retval float 电压值 (V)
 */
float ADC_CalculateVoltage(uint16_t raw_value)
{
    // STM32H7 ADC分辨率为16位，参考电压3.3V
    return (float)raw_value * 3.3f / 65535.0f;
}

/**
 * @brief 通过串口输出ADC通道数据
 * @retval None
 */
void ADC_PrintChannelData(void)
{
    // printf("\r\n=== ADC Channel Data ===\r\n");
    // printf("Timestamp: %lu ms\r\n", osKernelGetTickCount());
    // printf("%-10s | %-8s | %-8s | %-8s | %-8s\r\n", 
    //        "Channel", "Raw Avg", "Voltage", "Samples", "Status");
    // printf("-----------|----------|----------|----------|----------\r\n");
    
    // for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
    //     adc_channel_data_t *ch_data = &g_adc_system.channels[i];
    //     const char* status = ch_data->window_full ? "READY" : "FILLING";
        
    //     printf("%-10s | %8d | %8.3f | %8lu | %-8s\r\n",
    //            adc_channel_configs[i].name,
    //            ch_data->raw_average,
    //            ch_data->voltage_average,
    //            ch_data->sample_count,
    //            status);
    // }
    // printf("========================\r\n\n");
}

/**
 * @brief 输出ADC系统状态信息
 * @retval None
 */
void ADC_PrintSystemStatus(void)
{
    // printf("\r\n=== ADC System Status ===\r\n");
    // printf("Total Samples: %lu\r\n", g_adc_system.total_sample_count);
    // printf("Error Count: %d\r\n", g_adc_system.error_count);
    // printf("Sampling Period: %d ms\r\n", ADC_SAMPLE_PERIOD_MS);
    // printf("Window Size: %d samples\r\n", ADC_SAMPLE_WINDOW_SIZE);
    // printf("Active Channels: %d\r\n", ADC_CHANNEL_COUNT);
    // printf("Sampling Status: %s\r\n", g_adc_system.sampling_active ? "ACTIVE" : "INACTIVE");
    // printf("Free Heap: %lu bytes\r\n", xPortGetFreeHeapSize());
    // printf("System Uptime: %lu seconds\r\n", osKernelGetTickCount() / 1000);
    // printf("=========================\r\n\n");
}

/**
 * @brief 获取通道名称
 * @param channel_index 通道索引
 * @retval const char* 通道名称
 */
const char* ADC_GetChannelName(uint8_t channel_index)
{
    if (channel_index >= ADC_CHANNEL_COUNT) {
        return "INVALID";
    }
    return adc_channel_configs[channel_index].name;
}

/**
 * @brief 获取通道HAL定义
 * @param channel_index 通道索引
 * @retval uint32_t HAL通道定义
 */
uint32_t ADC_GetChannelHAL(uint8_t channel_index)
{
    if (channel_index >= ADC_CHANNEL_COUNT) {
        return 0;
    }
    return adc_channel_configs[channel_index].channel;
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
                   adc_channel_configs[i].name,
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
 * @brief 处理示波器模式输出
 * @param data ADC系统数据指针
 * @retval None
 */
void ADC_ProcessOscilloscopeOutput(const adc_system_data_t *data)
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
    
    // 增加示波器采样计数
    g_oscilloscope_sample_count++;
    
    // 每达到存储深度时输出统计信息
    if (g_oscilloscope_sample_count >= OSCILLOSCOPE_BUFFER_DEPTH) {
        printf("# Oscilloscope buffer full (%d samples), Total: %lu, Errors: %d\n", 
               OSCILLOSCOPE_BUFFER_DEPTH, data->total_sample_count, data->error_count);
        g_oscilloscope_sample_count = 0;
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
    
    // 增加示波器采样计数
    g_oscilloscope_sample_count++;
    
    // 每达到存储深度时输出统计信息（带#前缀，避免与图片数据混淆）
    if (g_oscilloscope_sample_count >= OSCILLOSCOPE_BUFFER_DEPTH) {
        printf("# Raw data buffer full (%d samples), Total: %lu, Errors: %d\n", 
               OSCILLOSCOPE_BUFFER_DEPTH, data->total_sample_count, data->error_count);
        g_oscilloscope_sample_count = 0;
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
        g_oscilloscope_sample_count = 0; // 重置示波器计数
        
        const char* mode_str;
        switch (mode) {
            case ADC_OUTPUT_MODE_DEBUG:
                mode_str = "DEBUG";
                break;
            case ADC_OUTPUT_MODE_OSCILLOSCOPE:
                mode_str = "OSCILLOSCOPE";
                break;
            case ADC_OUTPUT_MODE_RAW_DATA:
                mode_str = "RAW_DATA";
                break;
            default:
                mode_str = "UNKNOWN";
                break;
        }
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
    adc_output_mode_t next_mode = (g_adc_output_mode + 1) % ADC_OUTPUT_MODE_MAX;
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
    printf("  1 - OSCILLOSCOPE: Format 'channels: ch0,ch1,ch2,ch3,ch4,ch5\\n'\n");
    printf("  2 - RAW_DATA: Format 'ch0,ch1,ch2,ch3,ch4,ch5\\n'\n");
    printf("Current mode: %d (%s)\n", g_adc_output_mode, 
           g_adc_output_mode == ADC_OUTPUT_MODE_DEBUG ? "DEBUG" : 
           g_adc_output_mode == ADC_OUTPUT_MODE_OSCILLOSCOPE ? "OSCILLOSCOPE" : "RAW_DATA");
    printf("Buffer depth: %d samples\n", OSCILLOSCOPE_BUFFER_DEPTH);
    printf("Channel count: %d\n", ADC_CHANNEL_COUNT);
    printf("============================\n\n");
}

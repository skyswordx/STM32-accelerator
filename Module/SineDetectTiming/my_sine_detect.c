#include "my_sine_detect.h"
#include <string.h>
#include <math.h>

// 模块配置
sine_detect_config_t g_sine_config;

// 模块结果
sine_detect_result_t g_sine_result;

// 内部缓冲区（静态分配）
#define MAX_DATA_LENGTH 4096  // 最大数据长度
float32_t g_filtered_data[MAX_DATA_LENGTH];
peak_result_t g_peak_buffer[MAX_DATA_LENGTH];
edge_result_t g_edge_buffer[MAX_DATA_LENGTH];

// 内部函数声明
static void moving_average_filter(const float32_t* input, float32_t* output, uint32_t length, uint32_t filter_length);
static uint32_t find_peaks(const float32_t* data, peak_result_t* peaks, uint32_t max_peaks, uint32_t length);
static void parabolic_interpolation(const float32_t* data, peak_result_t* peak);
static float32_t calculate_midpoint(const float32_t* data, uint32_t length);
static uint32_t find_edges(const float32_t* data, edge_result_t* edges, uint32_t max_edges, uint32_t length, float32_t midpoint);
static void linear_interpolation_zero_crossing(const float32_t* data, edge_result_t* edge, uint32_t index, float32_t midpoint);

/**
 * @brief 处理ADC数据，检测峰值和边沿
 * @param adc_data 输入的ADC数据数组
 * @param result 输出的检测结果
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_process(const float32_t* adc_data, sine_detect_result_t* result)
{
    // 检查输入参数
    if (adc_data == NULL || result == NULL) {
        return -1;
    }
    
    // 数据预处理
    if (g_sine_config.enable_filter) {
        moving_average_filter(adc_data, g_filtered_data, g_sine_config.data_length, g_sine_config.filter_length);
    } else {
        memcpy(g_filtered_data, adc_data, sizeof(float32_t) * g_sine_config.data_length);
    }
    
    // 计算信号中点
    g_sine_result.signal_midpoint = calculate_midpoint(g_filtered_data, g_sine_config.data_length);
    
    // 峰值检测
    g_sine_result.peak_count = find_peaks(g_filtered_data, g_sine_result.peaks, g_sine_config.data_length, g_sine_config.data_length);
    
    // 边沿检测
    g_sine_result.edge_count = find_edges(g_filtered_data, g_sine_result.edges, g_sine_config.data_length, g_sine_config.data_length, g_sine_result.signal_midpoint);
    
    // 返回结果
    *result = g_sine_result;
    
    return 0;
}

/**
 * @brief 获取峰值检测结果
 * @param peaks 峰值结果数组
 * @param max_count 最大峰值数量
 * @return 实际检测到的峰值数量
 */
uint32_t my_sine_detect_get_peaks(peak_result_t* peaks, uint32_t max_count)
{
    if (peaks == NULL) {
        return 0;
    }
    
    uint32_t count = (g_sine_result.peak_count < max_count) ? g_sine_result.peak_count : max_count;
    memcpy(peaks, g_sine_result.peaks, sizeof(peak_result_t) * count);
    return count;
}

/**
 * @brief 获取边沿检测结果
 * @param edges 边沿结果数组
 * @param max_count 最大边沿数量
 * @return 实际检测到的边沿数量
 */
uint32_t my_sine_detect_get_edges(edge_result_t* edges, uint32_t max_count)
{
    if (edges == NULL) {
        return 0;
    }
    
    uint32_t count = (g_sine_result.edge_count < max_count) ? g_sine_result.edge_count : max_count;
    memcpy(edges, g_sine_result.edges, sizeof(edge_result_t) * count);
    return count;
}

/**
 * @brief 获取信号中点
 * @return 信号中点值
 */
float32_t my_sine_detect_get_midpoint(void)
{
    return g_sine_result.signal_midpoint;
}

/**
 * @brief 释放模块资源
 */
void my_sine_detect_deinit(void)
{
    g_sine_result.peaks = NULL;
    g_sine_result.edges = NULL;
    g_sine_result.peak_count = 0;
    g_sine_result.edge_count = 0;
    g_sine_result.signal_midpoint = 0.0f;
}

/**
 * @brief 移动平均滤波器
 * @param input 输入数据
 * @param output 输出数据
 * @param length 数据长度
 * @param filter_length 滤波器长度
 */
static void moving_average_filter(const float32_t* input, float32_t* output, uint32_t length, uint32_t filter_length)
{
    if (filter_length > length) {
        filter_length = length;
    }
    
    // 处理前filter_length-1个点
    for (uint32_t i = 0; i < filter_length - 1; i++) {
        float32_t sum = 0.0f;
        for (uint32_t j = 0; j <= i; j++) {
            sum += input[j];
        }
        output[i] = sum / (i + 1);
    }
    
    // 处理中间的点
    for (uint32_t i = filter_length - 1; i < length; i++) {
        float32_t sum = 0.0f;
        for (uint32_t j = 0; j < filter_length; j++) {
            sum += input[i - j];
        }
        output[i] = sum / filter_length;
    }
}

/**
 * @brief 查找峰值
 * @param data 输入数据
 * @param peaks 峰值结果数组
 * @param max_peaks 最大峰值数量
 * @param length 数据长度
 * @return 实际检测到的峰值数量
 */
static uint32_t find_peaks(const float32_t* data, peak_result_t* peaks, uint32_t max_peaks, uint32_t length)
{
    uint32_t count = 0;
    
    // 遍历数据查找峰值
    for (uint32_t i = 1; i < length - 1; i++) {
        // 检查是否还有空间存储峰值
        if (count >= max_peaks) {
            break;
        }
        
        // 正峰值条件
        if (data[i] > data[i - 1] && data[i] > data[i + 1]) {
            peaks[count].amplitude = data[i];
            peaks[count].position = (float32_t)i;
            peaks[count].index = i;
            peaks[count].is_positive = 1;
            
            // 抛物线插值
            parabolic_interpolation(data, &peaks[count]);
            
            count++;
        }
        // 负峰值条件
        else if (data[i] < data[i - 1] && data[i] < data[i + 1]) {
            peaks[count].amplitude = data[i];
            peaks[count].position = (float32_t)i;
            peaks[count].index = i;
            peaks[count].is_positive = 0;
            
            // 抛物线插值
            parabolic_interpolation(data, &peaks[count]);
            
            count++;
        }
    }
    
    return count;
}

/**
 * @brief 抛物线插值
 * @param data 输入数据
 * @param peak 峰值结果
 */
static void parabolic_interpolation(const float32_t* data, peak_result_t* peak)
{
    uint32_t index = peak->index;
    
    // 检查边界条件
    if (index == 0 || index >= g_sine_config.data_length - 1) {
        return;
    }
    
    // 获取相邻点的值
    float32_t y1 = data[index - 1];
    float32_t y2 = data[index];
    float32_t y3 = data[index + 1];
    
    // 计算抛物线插值
    float32_t delta = 0.5f * (y1 - y3) / (y1 - 2.0f * y2 + y3);
    
    // 更新峰值位置和幅度
    peak->position = (float32_t)index + delta;
    peak->amplitude = y2 - 0.25f * (y1 - y3) * delta;
}

/**
 * @brief 计算信号中点
 * @param data 输入数据
 * @param length 数据长度
 * @return 信号中点值
 */
static float32_t calculate_midpoint(const float32_t* data, uint32_t length)
{
    float32_t max_val = data[0];
    float32_t min_val = data[0];
    
    // 查找最大值和最小值
    for (uint32_t i = 1; i < length; i++) {
        if (data[i] > max_val) {
            max_val = data[i];
        }
        if (data[i] < min_val) {
            min_val = data[i];
        }
    }
    
    // 计算中点
    return (max_val + min_val) / 2.0f;
}

/**
 * @brief 查找边沿
 * @param data 输入数据
 * @param edges 边沿结果数组
 * @param max_edges 最大边沿数量
 * @param length 数据长度
 * @param midpoint 信号中点
 * @return 实际检测到的边沿数量
 */
static uint32_t find_edges(const float32_t* data, edge_result_t* edges, uint32_t max_edges, uint32_t length, float32_t midpoint)
{
    uint32_t count = 0;
    
    // 遍历数据查找边沿
    for (uint32_t i = 1; i < length; i++) {
        // 检查是否还有空间存储边沿
        if (count >= max_edges) {
            break;
        }
        
        // 上升沿条件
        if (data[i - 1] < midpoint && data[i] >= midpoint) {
            edges[count].position = (float32_t)(i - 1);
            edges[count].index = i - 1;
            edges[count].is_rising = 1;
            
            // 线性插值
            linear_interpolation_zero_crossing(data, &edges[count], i - 1, midpoint);
            
            count++;
        }
        // 下降沿条件
        else if (data[i - 1] > midpoint && data[i] <= midpoint) {
            edges[count].position = (float32_t)(i - 1);
            edges[count].index = i - 1;
            edges[count].is_rising = 0;
            
            // 线性插值
            linear_interpolation_zero_crossing(data, &edges[count], i - 1, midpoint);
            
            count++;
        }
    }
    
    return count;
}

/**
 * @brief 线性插值过零检测
 * @param data 输入数据
 * @param edge 边沿结果
 * @param index 边沿索引
 * @param midpoint 信号中点
 */
static void linear_interpolation_zero_crossing(const float32_t* data, edge_result_t* edge, uint32_t index, float32_t midpoint)
{
    // 检查边界条件
    if (index >= g_sine_config.data_length - 1) {
        return;
    }
    
    // 获取相邻点的值
    float32_t y1 = data[index];
    float32_t y2 = data[index + 1];
    
    // 计算线性插值
    float32_t delta = (midpoint - y1) / (y2 - y1);
    
    // 更新边沿位置
    edge->position = (float32_t)index + delta;
}

/**
 * @brief 初始化正弦波检测模块配置
 * @param config_params 配置参数
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_init(const sine_detect_config_params_t *config_params)
{
    // 检查输入参数
    if (config_params == NULL) {
        return -1;
    }
    
    // 配置全局结构体
    g_sine_config.sample_rate = config_params->sample_rate;
    g_sine_config.data_length = config_params->data_length;
    g_sine_config.enable_filter = config_params->enable_filter;
    g_sine_config.filter_length = config_params->filter_length;
    
    return 0;
}

/**
 * @brief 启动正弦波检测
 * @param adc_data 输入的ADC数据数组
 * @param result 输出的检测结果
 * @return 0表示成功，其他值表示失败
 */
int my_sine_detect_start(const float32_t* adc_data, sine_detect_result_t* result)
{
    // 检查输入参数
    if (adc_data == NULL || result == NULL) {
        return -1;
    }
    
    // 检查数据长度是否超过最大值
    if (g_sine_config.data_length > MAX_DATA_LENGTH) {
        return -2;
    }
    
    // 初始化结果结构体
    g_sine_result.peaks = g_peak_buffer;
    g_sine_result.edges = g_edge_buffer;
    g_sine_result.peak_count = 0;
    g_sine_result.edge_count = 0;
    g_sine_result.signal_midpoint = 0.0f;
    
    // 处理ADC数据
    return my_sine_detect_process(adc_data, result);
}

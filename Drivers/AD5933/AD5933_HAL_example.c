/**
 * @file AD5933_HAL_example.c
 * @brief AD5933 HAL驱动使用示例
 * @author 
 * @date 2025-07-28
 */

#include "AD5933_HAL.h"
#include "stm32h7xx_hal.h"  // 根据您的STM32型号调整
#include <stdio.h>

// 外部声明I2C句柄，需要在main.c中定义
extern I2C_HandleTypeDef hi2c1;

/**
 * @brief AD5933 HAL驱动使用示例
 * @note 在调用此函数前，请确保I2C已经初始化
 */
void AD5933_HAL_Example(void)
{
    HAL_StatusTypeDef status;
    ImpeType impedance_result;
    float temperature;
    
    printf("AD5933 HAL驱动测试开始\r\n");
    
    // 1. 初始化AD5933
    status = AD5933_Init(&hi2c1);  // 假设使用hi2c1，请根据实际情况修改
    if (status != HAL_OK) {
        printf("AD5933初始化失败: %d\r\n", status);
        return;
    }
    printf("AD5933初始化成功\r\n");
    
    // 2. 设置配置参数
    AD5933_SetConfig(0x00,  // 内部时钟
                     0x01,  // x1增益
                     0x00); // 2Vpp输出
    
    AD5933_SetFrequency(30000,  // 30kHz起始频率
                        100,    // 100Hz频率增量
                        10);    // 10个扫描点
    
    AD5933_SetSettlingTime(0x3F);  // 稳定时间
    
    // 3. 初始化频率扫描
    status = AD5933_FreInit(30000.0f, 100.0f);
    if (status != HAL_OK) {
        printf("频率初始化失败: %d\r\n", status);
        return;
    }
    printf("频率初始化成功\r\n");
    
    // 4. 执行阻抗测量
    printf("开始阻抗扫描...\r\n");
    for (int i = 0; i < 10; i++) {
        // 执行一次测量
        status = AD5933_StartOnceTest(&impedance_result, (i == 0) ? 1 : 1);
        if (status != HAL_OK) {
            printf("第%d次测量失败: %d\r\n", i + 1, status);
            continue;
        }
        
        // 计算当前频率
        float current_freq = 30000.0f + i * 100.0f;
        
        printf("频率: %.1f Hz, ", current_freq);
        printf("实部: %d, 虚部: %d, ", impedance_result.Re, impedance_result.Im);
        printf("阻抗: %.2f Ω, 相位: %.2f°\r\n", 
               impedance_result.Impedance, 
               impedance_result.Phase * 180.0f / PI);
        
        HAL_Delay(100);  // 延时100ms
    }
    
    // 5. 测量温度
    temperature = AD5933_Temperature_Test();
    if (temperature != -999.0f) {
        printf("芯片温度: %.2f°C\r\n", temperature);
    } else {
        printf("温度测量失败\r\n");
    }
    
    printf("AD5933测试完成\r\n");
}

/**
 * @brief 单点阻抗测量示例
 * @param frequency 测量频率 (Hz)
 * @param result 阻抗结果指针
 * @return HAL状态
 */
HAL_StatusTypeDef AD5933_SinglePointMeasurement(float frequency, ImpeType *result)
{
    HAL_StatusTypeDef status;
    
    if (result == NULL) {
        return HAL_ERROR;
    }
    
    // 设置单点测量参数
    AD5933_SetFrequency((uint32_t)frequency, 0, 1);  // 单点测量
    
    // 初始化频率
    status = AD5933_FreInit(frequency, 0.0f);
    if (status != HAL_OK) {
        return status;
    }
    
    // 执行测量
    return AD5933_StartOnceTest(result, 1);
}

/**
 * @brief 自定义频率扫描示例
 * @param start_freq 起始频率 (Hz)
 * @param end_freq 结束频率 (Hz)
 * @param num_points 扫描点数
 * @param results 结果数组指针
 * @return HAL状态
 */
HAL_StatusTypeDef AD5933_CustomFrequencySweep(float start_freq, float end_freq, 
                                              uint16_t num_points, ImpeType *results)
{
    HAL_StatusTypeDef status;
    float freq_increment;
    
    if (results == NULL || num_points == 0) {
        return HAL_ERROR;
    }
    
    // 计算频率增量
    if (num_points > 1) {
        freq_increment = (end_freq - start_freq) / (num_points - 1);
    } else {
        freq_increment = 0;
    }
    
    // 设置扫描参数
    AD5933_SetFrequency((uint32_t)start_freq, (uint32_t)freq_increment, num_points);
    
    // 初始化频率扫描
    status = AD5933_FreInit(start_freq, freq_increment);
    if (status != HAL_OK) {
        return status;
    }
    
    // 执行扫描
    for (int i = 0; i < num_points; i++) {
        status = AD5933_StartOnceTest(&results[i], (i == 0) ? 1 : 1);
        if (status != HAL_OK) {
            return status;
        }
        
        HAL_Delay(10);  // 测量间隔
    }
    
    return HAL_OK;
}

/**
 * @brief 校准示例函数
 * @note 这个函数展示了如何使用已知阻抗值进行校准
 */
void AD5933_CalibrationExample(void)
{
    ImpeType calibration_result;
    HAL_StatusTypeDef status;
    float known_impedance = 1000.0f;  // 已知校准电阻值 (1kΩ)
    
    printf("开始校准程序...\r\n");
    
    // 使用已知阻抗进行单点测量
    status = AD5933_SinglePointMeasurement(10000.0f, &calibration_result);  // 10kHz校准
    if (status == HAL_OK) {
        float gain_factor = known_impedance / calibration_result.Impedance;
        printf("校准增益因子: %.4f\r\n", gain_factor);
        printf("校准完成\r\n");
        
        // 在实际应用中，您需要将gain_factor保存到全局变量或EEPROM中
        // 然后在后续测量中使用这个因子来校正结果
    } else {
        printf("校准失败\r\n");
    }
}

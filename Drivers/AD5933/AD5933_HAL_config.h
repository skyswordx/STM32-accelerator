/**
 * @file AD5933_HAL_config.h
 * @brief AD5933 HAL驱动配置文件
 * @note 根据您的STM32型号调整相应的头文件包含
 */

#ifndef AD5933_HAL_CONFIG_H
#define AD5933_HAL_CONFIG_H

/* STM32系列选择 - 请根据您的项目取消注释对应的行 */

/* STM32F4系列 */
// #include "stm32f4xx_hal.h"

/* STM32F7系列 */
// #include "stm32f7xx_hal.h"

/* STM32H7系列 */
#include "stm32h7xx_hal.h"

/* STM32L4系列 */
// #include "stm32l4xx_hal.h"

/* STM32G4系列 */
// #include "stm32g4xx_hal.h"

/* STM32F1系列 */
// #include "stm32f1xx_hal.h"

/* AD5933配置参数 */
#define AD5933_I2C_TIMEOUT          1000    // I2C超时时间(ms)
#define AD5933_MAX_WAIT_COUNT        1000    // 最大等待次数
#define AD5933_TEMP_WAIT_COUNT       200     // 温度测量等待次数

/* 调试输出配置 */
#ifdef DEBUG
    #include <stdio.h>
    #define AD5933_DEBUG_PRINTF(...)    printf(__VA_ARGS__)
#else
    #define AD5933_DEBUG_PRINTF(...)    ((void)0)
#endif

/* 错误代码定义 */
typedef enum {
    AD5933_OK = 0,
    AD5933_ERROR = 1,
    AD5933_TIMEOUT = 2,
    AD5933_INVALID_PARAM = 3,
    AD5933_NOT_INITIALIZED = 4
} AD5933_StatusTypeDef;

#endif /* AD5933_HAL_CONFIG_H */

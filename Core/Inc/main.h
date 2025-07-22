/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define S_SDO_Pin GPIO_PIN_14
#define S_SDO_GPIO_Port GPIOD
#define PS0_Pin GPIO_PIN_6
#define PS0_GPIO_Port GPIOC
#define AD9954_IOSY_Pin GPIO_PIN_7
#define AD9954_IOSY_GPIO_Port GPIOC
#define AD9954_PWR_Pin GPIO_PIN_8
#define AD9954_PWR_GPIO_Port GPIOC
#define IOUPDATE_Pin GPIO_PIN_9
#define IOUPDATE_GPIO_Port GPIOC
#define AD9954_RES_Pin GPIO_PIN_8
#define AD9954_RES_GPIO_Port GPIOA
#define S_DIO_Pin GPIO_PIN_9
#define S_DIO_GPIO_Port GPIOA
#define S_SCLK_Pin GPIO_PIN_10
#define S_SCLK_GPIO_Port GPIOA
#define S_CS_Pin GPIO_PIN_11
#define S_CS_GPIO_Port GPIOA
#define PS1_Pin GPIO_PIN_12
#define PS1_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

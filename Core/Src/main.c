/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "math.h"
// #include "ADCsampleTask.h"
// #include "ADCOutputTask.h"
#include "AD9954.h"
#include "INA226.h"
#include "arm_math.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

#define USE_DUAL_ADC_INTERLEAVED 1 // 使用双ADC交错模式
#define USE_AD9954 0 // 使用AD9954 DDS芯片

/* 频谱数据结构 */
typedef struct {
  float frequency;    // 频率
  float magnitude;    // 幅度
  uint32_t bin_index; // FFT bin 索引
} spectrum_data_t;

/* 基波分量结果结构 */
typedef struct {
  float fundamental_frequency;  // 基波频率 (Hz)
  float fundamental_magnitude;  // 基波幅度
  uint32_t fundamental_index;   // 基波在频谱数组中的索引
  uint8_t found;               // 是否找到基波 (1=找到, 0=未找到)
} fundamental_result_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

DMA_HandleTypeDef hdma_bdma_generator0;
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
/* Variables for ADC dual mode DMA testing */
#define ADC_BUFFER_SIZE 1024 * 16 
#define ADC_8BIT_RESOLUTION 256.0f // 8位ADC分辨率

/* FFT相关变量定义 */
#define FFT_LENGTH ADC_BUFFER_SIZE //FFT长度
arm_cfft_radix4_instance_f32 scfft;//定义scfft结构
float FFT_InputBuf[FFT_LENGTH*2];  //FFT输入数组（复数形式：实部+虚部）
float magnitude_array[FFT_LENGTH/2];  //幅度谱数组（只保留有效频谱范围）

/* 双缓冲机制 - 使用链接器自动分配内避免地址冲突 */
uint16_t dmabuffer_ping[ADC_BUFFER_SIZE] __attribute__((aligned(32))); // Ping buffer for DMA transfers - 32字节对齐
uint16_t dmabuffer_pong[ADC_BUFFER_SIZE] __attribute__((aligned(32))); // Pong buffer for DMA transfers - 32字节对齐

uint16_t* active_dma_buffer = dmabuffer_ping;     // 当前DMA写入的缓冲区
uint16_t* processing_buffer = dmabuffer_pong;     // 当前处理的缓冲区

/* 合并的ADC数据 - 优化：直接从DMA缓冲区填充，无需中间数组 */
uint16_t merged_adc_data[ADC_BUFFER_SIZE * 2]; // Buffer for merged interleaved ADC data
volatile uint8_t adc_conversion_complete = 0; // Flag to indicate conversion complete
volatile uint8_t buffer_swap_flag = 0; // Flag to indicate buffer swap is needed
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_BDMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC2_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
/* 函数声明 */
static void SwapDMABuffers(void);
static void ProcessCompleteBuffer(uint16_t* buffer);
static void PrintTimeDomainDataVOFA(uint16_t* merged_data, uint32_t sample_count);
static void PrintFrequencySpectrumVOFA(float actual_sampling_rate, uint8_t remove_dc, float shi);
static float ADC_ToVoltage(uint16_t adc_value);
static void BuildMagnitudeArray(float actual_sampling_rate, uint8_t remove_dc, float shi);
static fundamental_result_t FindFundamentalComponent(float min_freq, float max_freq);
static void ProcessFrequencyDomain(uint16_t* adc_data, uint32_t data_length, uint32_t update_interval_ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int fputc(int ch, FILE *f)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xffff);
  return ch;
}
int fgetc(FILE * f)
{
  uint8_t ch = 0;
  HAL_UART_Receive(&huart1,&ch, 1, 0xffff);
  return ch;
}

/**
  * @brief  ADC conversion complete callback in non-blocking mode
  * @param  hadc: ADC handle
  * @retval None
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  /* ADC转换完成回调 */
  if(hadc->Instance == ADC1)
  {
    /* 设置缓冲区交换标志 */
    buffer_swap_flag = 1;
    /* 仅在中断中设置标志，避免在中断上下文中进行复杂操作 */
    adc_conversion_complete = 1;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_BDMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_ADC2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  
  #if USE_AD9954
    AD9954_Init();
    AD9954_Set_Fre(1000.0);
    AD9954_Set_Amp(16383);
    AD9954_Set_Phase(0);
  #endif

  /* 清空DMA缓冲区并确保缓存一致 */
  memset(dmabuffer_ping, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  memset(dmabuffer_pong, 0, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  /* 清除DMA缓冲区的D-Cache，确保DMA能够正确写入 */
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_ping, ADC_BUFFER_SIZE * sizeof(uint16_t));
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_pong, ADC_BUFFER_SIZE * sizeof(uint16_t));

  HAL_TIM_Base_Start(&htim3); // 启动定时器3作为时间戳基准

  #if USE_DUAL_ADC_INTERLEAVED
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);

    /* 启动ADC2 */
    HAL_ADC_Start(&hadc2);
    /* 启动ADC双通道模式DMA传输 */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
  #endif

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  // ďż?? ADC çĺ¤éééć ˇĺčžĺşďźä¸éďż??
  // ADCSamplingTaskHandle = osThreadNew(ADCSamplingTask, NULL, &ADCSamplingTask_attributes);
  // ADCOutputTaskHandle = osThreadNew(ADCOutputTask, NULL, &ADCOutputTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
 
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInitStruct.PLL2.PLL2M = 4;
  PeriphClkInitStruct.PLL2.PLL2N = 10;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_TRGO;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_8B;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_DUALMODE_INTERL;
  multimode.DualModeData = ADC_DUALMODEDATAFORMAT_8_BITS;
  multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc2.Init.OversamplingMode = DISABLE;
  hadc2.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_8B;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10C0ECFF;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 200-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 10-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  * Configure DMA for memory to memory transfers
  *   hdma_bdma_generator0
  */
static void MX_BDMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_BDMA_CLK_ENABLE();

  /* Configure DMA request hdma_bdma_generator0 on BDMA_Channel0 */
  hdma_bdma_generator0.Instance = BDMA_Channel0;
  hdma_bdma_generator0.Init.Request = BDMA_REQUEST_GENERATOR0;
  hdma_bdma_generator0.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_bdma_generator0.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_bdma_generator0.Init.MemInc = DMA_MINC_ENABLE;
  hdma_bdma_generator0.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_bdma_generator0.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_bdma_generator0.Init.Mode = DMA_NORMAL;
  hdma_bdma_generator0.Init.Priority = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&hdma_bdma_generator0) != HAL_OK)
  {
    Error_Handler( );
  }

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(AD9954_OSK_GPIO_Port, AD9954_OSK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, PS0_Pin|AD9954_IOSY_Pin|AD9954_PWR_Pin|IOUPDATE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, AD9954_RES_Pin|S_DIO_Pin|S_SCLK_Pin|S_CS_Pin
                          |PS1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : AD9954_OSK_Pin */
  GPIO_InitStruct.Pin = AD9954_OSK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(AD9954_OSK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : S_SDO_Pin */
  GPIO_InitStruct.Pin = S_SDO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(S_SDO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PS0_Pin AD9954_IOSY_Pin AD9954_PWR_Pin IOUPDATE_Pin */
  GPIO_InitStruct.Pin = PS0_Pin|AD9954_IOSY_Pin|AD9954_PWR_Pin|IOUPDATE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : AD9954_RES_Pin S_DIO_Pin S_SCLK_Pin S_CS_Pin
                           PS1_Pin */
  GPIO_InitStruct.Pin = AD9954_RES_Pin|S_DIO_Pin|S_SCLK_Pin|S_CS_Pin
                          |PS1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief 交换DMA缓冲�???
 * @retval None
 */
static void SwapDMABuffers(void)
{
  #if USE_DUAL_ADC_INTERLEAVED
    /* 停止当前DMA传输 */
    HAL_ADCEx_MultiModeStop_DMA(&hadc1);
  #endif
  
  /* 交换缓冲区指针 */
  uint16_t* temp = active_dma_buffer;
  active_dma_buffer = processing_buffer;
  processing_buffer = temp;
  
  #if USE_DUAL_ADC_INTERLEAVED
    /* 重新启动DMA传输到新的活动缓冲区 */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
  #endif
}

/**
 * @brief 处理完整的缓冲区数据
 * @param buffer 要处理的缓冲区指�???
 * @retval None
 */
static void ProcessCompleteBuffer(uint16_t* buffer)
{
  /* 在主任务中进行缓存操作，避免在中断上下文中的时序问题 */
  /* 刷新DMA缓冲区的D-Cache */
  SCB_InvalidateDCache_by_Addr((uint32_t*)buffer, ADC_BUFFER_SIZE * sizeof(uint16_t));
  
  #if USE_DUAL_ADC_INTERLEAVED
     /* 直接从DMA缓冲区解包数据到merged_adc_data数组，避免中间数组：
      * buffer[j] = 0xXXYY (16位)
      * 其中 XX (低8位) = ADC2 (slave)
      * 其中 YY (高8位) = ADC1 (master)
      * 交替排列为：merged_data[0] = ADC1[0], merged_data[1] = ADC2[0]
      * merged_data[2] = ADC1[1], merged_data[3] = ADC2[1]
      * ...
      * 这样可以实现双通道的有效采样
      */
      for(uint32_t j = 0; j < ADC_BUFFER_SIZE; j++)
      {
        merged_adc_data[j * 2] = (uint16_t)(buffer[j] & 0xFF);        // 偶数索引放ADC1数据 (低8位)
        merged_adc_data[j * 2 + 1] = (uint16_t)((buffer[j] >> 8) & 0xFF); // 奇数索引放ADC2数据 (高8位)
      }
      
      /* 调用频域处理函数，每隔1000毫秒（1秒）更新一次频谱分析 */
      ProcessFrequencyDomain(merged_adc_data, ADC_BUFFER_SIZE * 2, 1000);
      
      /* 可选的输出操作 */
      PrintTimeDomainDataVOFA(merged_adc_data, ADC_BUFFER_SIZE * 2);
      // PrintFrequencySpectrumVOFA(actual_sampling_rate, remove_dc, shi);

  #endif
}

/**
 * @brief 按照VOFA协议输出时域波形数据
 * @param merged_data 合并后的交替采样数据缓冲区
 * @param sample_count 总样本数 (每个样本包含电压和时间戳)
 * @retval None
 */
static void PrintTimeDomainDataVOFA(uint16_t* merged_data, uint32_t sample_count)
{
  static uint32_t sample_index = 0; // 静态变量保存样本索引

  /* 输出交替采样数据，每个样本包含电压和时间戳 */
  for(uint32_t i = 0; i < sample_count; i++)
  {
    float voltage = ADC_ToVoltage(merged_data[i]);
    
    /* 计算每个样本的时间戳（微秒）
     * 使用样本索引来计算时间戳，假设每个样本间隔固定
     * 这里假设交替采样的有效采样率为20kHz（每个样本间隔50微秒）
     */
    uint32_t timestamp = sample_index * 50; // 每个样本间隔50微秒
    
    /* VOFA协议格式：voltage,timestamp */
    printf("%.6f,%lu\n", voltage, timestamp);
    
    sample_index++;
  }
}

/**
 * @brief 将ADC原始值转换为电压值
 * @param adc_value ADC原始值 (8位)
 * @retval float 电压值 (V)
 */
static float ADC_ToVoltage(uint16_t adc_value)
{
  /* 8位ADC，参考电压3.3V */
  return (float)adc_value * 3.3f / ADC_8BIT_RESOLUTION;
}

/**
 * @brief 按照VOFA协议输出频谱数据
 * @param actual_sampling_rate 实际采样率 (Hz)
 * @param remove_dc 是否已滤除直流分量
 * @param shi 神秘系数
 * @retval None
 */
static void PrintFrequencySpectrumVOFA(float actual_sampling_rate, uint8_t remove_dc, float shi)
{
  if (remove_dc == 1) {
    printf("=== 频谱数据 (滤除直流分量) ===\n");
  } else {
    printf("=== 频谱数据 (包含直流分量) ===\n");
  }

  /* 计算幅度谱并输出 */
  // 全频谱，实际上有效频率范围只从 0 到采样率的 1/2（奈奎斯特频率）
  // 采样率的 1/2 到采样率本身的部分是镜像
  for(uint32_t i = 0; i < FFT_LENGTH; i++) {
    float32_t real = FFT_InputBuf[2 * i];
    float32_t imag = FFT_InputBuf[2 * i + 1];
    float32_t magnitude = sqrtf(real * real + imag * imag);
    
    /* 神秘公式 */
    /* 输出频率和对应的幅度值 */
    float frequency = shi * (float)i * actual_sampling_rate / FFT_LENGTH;
    printf("%.2f,%.6f\n", frequency, magnitude);
  }
  
  printf("=== 频谱数据结束 ===\n");
}

/**
 * @brief 构建幅度谱数组
 * @param actual_sampling_rate 实际采样率 (Hz)
 * @param remove_dc 是否已滤除直流分�???
 * @param shi 神秘系数
 * @retval None
 */
static void BuildMagnitudeArray(float actual_sampling_rate, uint8_t remove_dc, float shi)
{
  /* 只计算有效频谱范围 (0 到 采样率/2) */
  uint32_t valid_bins = FFT_LENGTH / 2;
  
  for(uint32_t i = 0; i < valid_bins; i++) {
    float32_t real = FFT_InputBuf[2 * i];
    float32_t imag = FFT_InputBuf[2 * i + 1];
    float32_t magnitude = sqrtf(real * real + imag * imag);

    /* 存储到幅度数组 */
    magnitude_array[i] = magnitude;
  }
}

/**
 * @brief 查找基波分量
 * @param min_freq 搜索的最小频率 (Hz)
 * @param max_freq 搜索的最大频率 (Hz)
 * @retval fundamental_result_t 基波查找结果
 */
static fundamental_result_t FindFundamentalComponent(float min_freq, float max_freq)
{
  fundamental_result_t result = {0};
  result.found = 0;
  
  /* 系统参数 */
  static float actual_sampling_rate = 10730000.0f; // 10.73 MHz
  static float shi = 0.09f; // 神秘系数
  
  /* 有效频谱范围 */
  uint32_t valid_bins = FFT_LENGTH / 2;
  
  /* 直接通过频率计算bin索引范围 - 避免循环查找 */
  uint32_t start_bin = (uint32_t)(min_freq * FFT_LENGTH / (shi * actual_sampling_rate));
  uint32_t end_bin = (uint32_t)(max_freq * FFT_LENGTH / (shi * actual_sampling_rate));
  
  /* 确保索引在有效范围内 */
  if (start_bin >= valid_bins) start_bin = valid_bins - 1;
  if (end_bin >= valid_bins) end_bin = valid_bins - 1;
  if (start_bin > end_bin) {
    printf("# 警告: 频率范围 %.2f - %.2f Hz 无效\n", min_freq, max_freq);
    return result;
  }

  /* 计算搜索范围的长度 */
  uint32_t search_length = end_bin - start_bin + 1;

  /* 使用ARM DSP库的arm_max_f32函数查找最大幅度值 */
  float max_magnitude;
  uint32_t max_index_relative;
  
  arm_max_f32(&magnitude_array[start_bin], search_length, &max_magnitude, &max_index_relative);
  
  /* 计算实际的bin索引 */
  uint32_t actual_bin_index = start_bin + max_index_relative;
  
  /* 计算基波频率 */
  float fundamental_frequency = shi * (float)actual_bin_index * actual_sampling_rate / FFT_LENGTH;
  
  /* 填充结果 */
  result.fundamental_frequency = fundamental_frequency;
  result.fundamental_magnitude = max_magnitude;
  result.fundamental_index = actual_bin_index;
  result.found = 1;
  
  printf("# 基波分量查找结果:\n");
  printf("# 频率: %.2f Hz, 幅度: %.6f, 索引: %d (搜索范围: %d-%d)\n", 
         result.fundamental_frequency, 
         result.fundamental_magnitude, 
         result.fundamental_index,
         start_bin, end_bin);
  
  return result;
}

/**
 * @brief 频域处理函数 - 对ADC数据进行FFT分析并更新全局频谱数组
 * @param adc_data ADC数据缓冲区指针
 * @param data_length ADC数据长度
 * @param update_interval_ms 更新间隔时间（毫秒）
 * @retval None
 */
static void ProcessFrequencyDomain(uint16_t* adc_data, uint32_t data_length, uint32_t update_interval_ms)
{
  static uint32_t last_update_time = 0;
  uint32_t current_time = HAL_GetTick();
  
  /* 检查是否到了更新时间 */
  if (current_time - last_update_time < update_interval_ms) {
    return; // 还没到更新时间，直接返回
  }
  
  /* 更新时间戳 */
  last_update_time = current_time;
  
  /* 系统参数 */
  static float actual_sampling_rate = 10730000.0f; // 10.73 MHz
  static float shi = 0.09f; // 神秘系数
  uint8_t remove_dc = 1; // 滤除直流分量
  
  /* 直接在全局FFT_InputBuf中准备FFT数据 */
  if (remove_dc == 1) {
    /* 计算直流分量 */
    float dc_component = 0.0f;
    for(uint32_t i = 0; i < data_length; i++) {
      dc_component += adc_data[i];
    }
    dc_component /= data_length;
    
    /* 填充FFT输入缓冲区并滤除直流分量 */
    uint32_t fft_samples = (data_length < FFT_LENGTH) ? data_length : FFT_LENGTH;
    for(uint32_t i = 0; i < fft_samples; i++) {
      FFT_InputBuf[2*i] = (adc_data[i] - dc_component) * 3.3f / ADC_8BIT_RESOLUTION; // 实部
      FFT_InputBuf[2*i+1] = 0.0f; // 虚部
    }
    
    /* 如果数据长度小于FFT长度，用零填充剩余部分 */
    for(uint32_t i = fft_samples; i < FFT_LENGTH; i++) {
      FFT_InputBuf[2*i] = 0.0f;
      FFT_InputBuf[2*i+1] = 0.0f;
    }
  } else {
    /* 填充FFT输入缓冲区，保留直流分量 */
    uint32_t fft_samples = (data_length < FFT_LENGTH) ? data_length : FFT_LENGTH;
    for(uint32_t i = 0; i < fft_samples; i++) {
      FFT_InputBuf[2*i] = adc_data[i] * 3.3f / ADC_8BIT_RESOLUTION; // 实部
      FFT_InputBuf[2*i+1] = 0.0f; // 虚部
    }
    
    /* 如果数据长度小于FFT长度，用零填充剩余部分 */
    for(uint32_t i = fft_samples; i < FFT_LENGTH; i++) {
      FFT_InputBuf[2*i] = 0.0f;
      FFT_InputBuf[2*i+1] = 0.0f;
    }
  }
  
  /* 初始化并执行FFT */
  arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH, 0, 1);
  arm_cfft_radix4_f32(&scfft, FFT_InputBuf);
  
  /* 计算幅度谱并更新全局数组 */
  BuildMagnitudeArray(actual_sampling_rate, remove_dc, shi);
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  
  /* Infinite loop */
  for(;;)
  {
    /* Check if ADC conversion is complete */
    if(adc_conversion_complete)
    {
      /* Reset the flag */
      adc_conversion_complete = 0;

      /* 处理缓冲区交换 */
      if(buffer_swap_flag)
      {
        buffer_swap_flag = 0;
        SwapDMABuffers();
      }
      
      /* 处理完整的缓冲区 */
      ProcessCompleteBuffer(processing_buffer);

    }
    
    osDelay(1);  /* Small delay to prevent CPU hogging */
  }
  /* USER CODE END 5 */
}

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

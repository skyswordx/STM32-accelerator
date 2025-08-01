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
// #include "ADCsampleTask.h"
// #include "ADCOutputTask.h"
#include "AD9954.h"
#include "INA226.h"
#include "arm_math.h"
#include "string.h"
#include <math.h>
#include "filter_identification.h" 
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* ADC system data structure for message queue */
typedef struct {
  uint16_t adc1_value;
  uint16_t adc2_value;
} adc_system_data_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define SYSTEM_SAMPLING_RATE_HZ   200000.0f      
#ifndef PI
#define PI 3.141592653589793f
#endif
/* 全域變數 */
ContinuousTransferFunction identified_tf;               // 用於儲存辨識結果
arm_biquad_casd_df1_inst_f32 iir_filter_instance;       // CMSIS-DSP IIR 濾波器實例
float32_t iir_coeffs[5];                                // [b0, b1, b2, -a1, -a2] - CMSIS-DSP 格式
float32_t iir_state[4];                                 // IIR 狀態變數

static void run_system_identification_test(void);
static void discretize_tf_bilinear(const ContinuousTransferFunction* tf_s, float32_t Ts, float32_t* coeffs_z);


// 測試流程函式
static void run_full_filter_test_suite(void);
static void test_lpf(void);
static void test_hpf(void);
static void test_bpf(void);
static void test_bsf(void);

// 數據生成函式
static void generate_simulated_lpf_data(float32_t* w, float32_t* h, float32_t fc);
static void generate_simulated_hpf_data(float32_t* w, float32_t* h, float32_t fc);
static void generate_simulated_bpf_data(float32_t* w, float32_t* h, float32_t f0, float32_t q);
static void generate_simulated_bsf_data(float32_t* w, float32_t* h, float32_t f0, float32_t q);

static void run_time_domain_simulation(const char* filter_type, float32_t f_char, float32_t q_factor);
static void calculate_analog_response(const char* type, float32_t f, float32_t f_char, float32_t q, float32_t* mag, float32_t* phase_rad);
static void vofa_output_wave_3ch(float32_t ch1, float32_t ch2, float32_t ch3);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
osMessageQueueId_t ADCQueueHandle;

/* Variables for ADC dual mode DMA testing */
#define ADC_BUFFER_SIZE 1024

/* 双缓冲机制 - 使用链接器自动分配内存，避免地址冲突 */
uint32_t dmabuffer_ping[ADC_BUFFER_SIZE] __attribute__((aligned(32))); // Ping buffer for DMA transfers - 32字节对齐
uint32_t dmabuffer_pong[ADC_BUFFER_SIZE] __attribute__((aligned(32))); // Pong buffer for DMA transfers - 32字节对齐

uint32_t* active_dma_buffer = dmabuffer_ping;     // 当前DMA写入的缓冲区
uint32_t* processing_buffer = dmabuffer_pong;     // 当前处理的缓冲区

uint16_t adc1_data[ADC_BUFFER_SIZE]; // Buffer for ADC1 data
uint16_t adc2_data[ADC_BUFFER_SIZE]; // Buffer for ADC2 data
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
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC2_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
/* 函数声明 */
static void SwapDMABuffers(void);
static void ProcessCompleteBuffer(uint32_t* buffer);
static void PrintInterleavedDataVOFA(uint16_t* merged_data, uint32_t sample_count);
static float ADC_ToVoltage(uint16_t adc_value);
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
  // MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  // MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
      /* 清除串口終端並打印歡迎信息 */
  printf("\033[2J\033[H"); // ANSI escape code to clear screen and move cursor to home
  printf("====================================================\r\n");
  printf(" STM32H7 Filter Identification Algorithm Test\r\n");
  printf("====================================================\r\n\r\n");
  // 執行完整的四種濾波器辨識與驗證流程
  run_full_filter_test_suite();

  printf("\r\n====================================================\r\n");
  printf("All filter tests finished. System halted.\r\n");
  printf("====================================================\r\n\r\n");

  
  // AD9954_Init();
	// AD9954_Set_Fre(1000.0);
	// AD9954_Set_Amp(16383);
	// AD9954_Set_Phase(0);

  /* Initialize and setup ADCs for dual mode DMA operation */
  // printf("Starting ADC dual mode interleaved sampling...\r\n");
  
  // /* Calibrate both ADCs */
  // if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  // {
  //   printf("ADC2 calibration error\r\n");
  //   Error_Handler();
  // }
  
  // if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  // {
  //   printf("ADC1 calibration error\r\n");
  //   Error_Handler();
  // }
  
  printf("ADC calibration complete\r\n");

  /* Print DMA buffer information */
  printf("DMA buffer addresses: Ping=0x%08lx, Pong=0x%08lx, size: %lu bytes each\r\n", 
         (unsigned long)dmabuffer_ping, 
         (unsigned long)dmabuffer_pong,
         (unsigned long)(ADC_BUFFER_SIZE * sizeof(uint32_t)));

  /* 清空DMA缓冲区并确保缓存一致性 */
  memset(dmabuffer_ping, 0, ADC_BUFFER_SIZE * sizeof(uint32_t));
  memset(dmabuffer_pong, 0, ADC_BUFFER_SIZE * sizeof(uint32_t));
  
  /* 清除DMA缓冲区的D-Cache，确保DMA能够正确写入 */
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_ping, ADC_BUFFER_SIZE * sizeof(uint32_t));
  SCB_CleanDCache_by_Addr((uint32_t*)dmabuffer_pong, ADC_BUFFER_SIZE * sizeof(uint32_t));
  
  /* 检查DMA缓冲区对齐 */
  // printf("DMA buffer alignment: Ping=%s, Pong=%s\r\n", 
  //        (((uint32_t)dmabuffer_ping & 0x1F) == 0) ? "yes" : "no",
  //        (((uint32_t)dmabuffer_pong & 0x1F) == 0) ? "yes" : "no");

  // /* 启动ADC2 */
  // printf("Starting ADC2...\r\n");
  // if (HAL_ADC_Start(&hadc2) != HAL_OK)
  // {
  //   printf("ADC2 start error\r\n");
  //   Error_Handler();
  // }
  // printf("ADC2 started successfully\r\n");
  
  // /* 启动ADC1的DMA多通道模式 */
  // printf("Starting ADC1 with DMA...\r\n");
  
  /* 设置DMA测试大小 */
  // uint32_t dma_test_size = ADC_BUFFER_SIZE; // 初始测试样本数
  // printf("DMA test size: %lu samples\r\n", (unsigned long)dma_test_size);
  
  /* 启动ADC双通道模式DMA传输 */
  // HAL_StatusTypeDef status = HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, dma_test_size);
  // if (status != HAL_OK)
  // {
  //   printf("ADC1 multimode DMA start error: %d\r\n", (int)status);
  //   Error_Handler();
  // }
  
  // printf("ADC dual mode interleaved sampling started successfully\r\n");

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
  // ADCQueueHandle = osMessageQueueNew(10, sizeof(adc_system_data_t), NULL);
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
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
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
  hadc2.Init.ContinuousConvMode = ENABLE;
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
 * @brief 交换DMA缓冲区
 * @retval None
 */
static void SwapDMABuffers(void)
{
  /* 停止当前DMA传输 */
  HAL_ADCEx_MultiModeStop_DMA(&hadc1);
  
  /* 交换缓冲区指针 */
  uint32_t* temp = active_dma_buffer;
  active_dma_buffer = processing_buffer;
  processing_buffer = temp;
  
  /* 重新启动DMA传输到新的活动缓冲区 */
  HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)active_dma_buffer, ADC_BUFFER_SIZE);
}

/**
 * @brief 处理完整的缓冲区数据
 * @param buffer 要处理的缓冲区指针
 * @retval None
 */
static void ProcessCompleteBuffer(uint32_t* buffer)
{
  /* 在主任务中进行缓存操作，避免在中断上下文中的时序问题 */
  /* 刷新DMA缓冲区的D-Cache */
  SCB_InvalidateDCache_by_Addr((uint32_t*)buffer, ADC_BUFFER_SIZE * sizeof(uint32_t));
  
  /* 8位模式下数据解包：
   * buffer[j] = 0x0000xxyy
   * 其中 xx (高8位) = ADC2 (slave)
   * 其中 yy (低8位) = ADC1 (master)
   */
  for(uint32_t j = 0; j < ADC_BUFFER_SIZE; j++)
  {
    adc1_data[j] = (uint16_t)(buffer[j] & 0xFF);        // 低8位是ADC1 (master)
    adc2_data[j] = (uint16_t)((buffer[j] >> 8) & 0xFF); // 高8位是ADC2 (slave)
  }
  
  /* 合并主从ADC数据为交替采样数据，实现双倍采样率效果：
   * 假设ADC1先采样，ADC2后采样，则交替排列为：
   * merged_data[0] = ADC1[0], merged_data[1] = ADC2[0]
   * merged_data[2] = ADC1[1], merged_data[3] = ADC2[1]
   * ...
   * 这样可以实现双倍的有效采样率
   */
  for(uint32_t i = 0; i < ADC_BUFFER_SIZE; i++)
  {
    merged_adc_data[i * 2] = adc1_data[i];     // 偶数索引放ADC1数据
    merged_adc_data[i * 2 + 1] = adc2_data[i]; // 奇数索引放ADC2数据
  }
  
  /* 输出交替采样数据到VOFA */
  PrintInterleavedDataVOFA(merged_adc_data, ADC_BUFFER_SIZE * 2);
}

/**
 * @brief 按照VOFA协议输出交替采样数据
 * @param merged_data 合并后的交替采样数据缓冲区
 * @param sample_count 总样本数量
 * @retval None
 */
static void PrintInterleavedDataVOFA(uint16_t* merged_data, uint32_t sample_count)
{
  static uint32_t sample_index = 0; // 静态变量保存样本索引
  
  /* 输出交替采样数据，每个样本包含电压值和时间戳 */
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
  return (float)adc_value * 3.3f / 255.0f;
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
  uint32_t last_check_tick = 0;
  uint32_t waveform_count = 0;
  
  printf("DefaultTask started, waiting for interleaved ADC data...\r\n");
  
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
      
      waveform_count++;
      
      /* 每10个波形输出一次统计信息（减少干扰） */
      uint32_t current_tick = osKernelGetTickCount();
      if(current_tick - last_check_tick >= 10000)
      {
        last_check_tick = current_tick;
        printf("# Interleaved samples processed: %lu, System time: %lu ms\r\n", 
               waveform_count * ADC_BUFFER_SIZE * 2, current_tick);
      }
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





// /**
//   * @brief  執行從數據生成到算法驗證的完整測試流程
//   */
// static void run_system_identification_test(void)
// {
//     // 靜態分配陣列以儲存頻率和響應數據
//     static float32_t w_rad[NUM_FREQ_POINTS];             // 角頻率 (rad/s)
//     static float32_t H_measured_cmplx[NUM_FREQ_POINTS * 2]; // 複數響應 [R,I,R,I...]

//     // =========================================================================
//     // 步驟 1: 生成模擬的測量數據 (H_measured 和 w_rad)
//     // =========================================================================
//     printf("STEP 1: Generating simulated test data...\r\n");
//     printf(" -> Simulating a 2nd-order Butterworth LPF.\r\n");
    
//     generate_simulated_lpf_data(w_rad, H_measured_cmplx);
    
//     printf(" -> Test data generated for %d frequency points.\r\n\r\n", NUM_FREQ_POINTS);

//     // =========================================================================
//     // 步驟 2: 呼叫辨識函式，對模擬數據進行擬合
//     // =========================================================================
//     printf("STEP 2: Running filter identification algorithm...\r\n");
    
//     identify_filter(&identified_tf, w_rad, H_measured_cmplx);

//     printf(" -> Identification finished.\r\n\r\n");
    
//     // =========================================================================
//     // 步驟 3: 顯示辨識出的連續時間傳遞函數 H(s) 結果
//     // =========================================================================
//     printf("STEP 3: Displaying continuous-time H(s) results...\r\n");
//     if (isnan(identified_tf.b0)) {
//         printf(" -> ERROR: Identification failed. The matrix solution might have failed.\r\n");
//         return;
//     }

//     const char* type_str[] = {"LPF", "HPF", "BPF", "BSF", "Unknown"};
//     printf(" -> Identified Filter Type: %s\r\n", type_str[identified_tf.identified_type]);
//     printf(" -> H(s) = (b2*s^2 + b1*s + b0) / (s^2 + a1*s + a0)\r\n");
//     printf(" -> Identified Coefficients:\r\n");
//     printf("    b2 = %e\r\n", identified_tf.b2);
//     printf("    b1 = %e\r\n", identified_tf.b1);
//     printf("    b0 = %e\r\n", identified_tf.b0);
//     printf("    a1 = %e\r\n", identified_tf.a1);
//     printf("    a0 = %e\r\n", identified_tf.a0);
//     printf("\r\n");

//     // =========================================================================
//     // 步驟 4: 將 H(s) 離散化為 H(z) 並準備 IIR 濾波器配置
//     // =========================================================================
//     printf("STEP 4: Discretizing to H(z) for digital implementation...\r\n");

//     const float32_t Ts = 1.0f / SYSTEM_SAMPLING_RATE_HZ;
//     discretize_tf_bilinear(&identified_tf, Ts, iir_coeffs);
    
//     printf(" -> Discretized H(z) coefficients (for CMSIS-DSP, Fs = %.1f kHz):\r\n", SYSTEM_SAMPLING_RATE_HZ / 1000.0f);
//     printf(" -> H(z) = (b0z + b1z*z^-1 + b2z*z^-2) / (1 + a1z*z^-1 + a2z*z^-2)\r\n");
//     printf("    b0z = %e\r\n", iir_coeffs[0]);
//     printf("    b1z = %e\r\n", iir_coeffs[1]);
//     printf("    b2z = %e\r\n", iir_coeffs[2]);
//     printf("    a1z = %e (Note: CMSIS uses coeff = -a1z)\r\n", -iir_coeffs[3]);
//     printf("    a2z = %e (Note: CMSIS uses coeff = -a2z)\r\n", -iir_coeffs[4]);

//     // 初始化 CMSIS-DSP IIR 濾波器實例 (作為演示)
//     arm_biquad_cascade_df1_init_f32(&iir_filter_instance, 1, iir_coeffs, iir_state);
//     printf(" -> CMSIS-DSP IIR filter instance initialized for verification.\r\n\r\n");
// }


// /**
//   * @brief  生成一個理想的二階 Butterworth 低通濾波器的頻率響應數據
//   * @param  w_rad_array: 指向用於儲存角頻率的陣列
//   * @param  h_cmplx_array: 指向用於儲存複數響應的陣列
//   * @retval None
//   */
// static void generate_simulated_lpf_data(float32_t* w_rad_array, float32_t* h_cmplx_array)
// {
//     // --- **修改點**: 使用一個較低的截止頻率，讓係數量級更合理 ---
//     const float32_t F_CUTOFF_HZ = 250.0f; // 截止頻率 250 Hz
//     const float32_t WN = 2.0f * PI * F_CUTOFF_HZ;
//     const float32_t ZETA = 0.70710678f; // Butterworth 濾波器的阻尼比

//     const float32_t a1_true = 2.0f * ZETA * WN;
//     const float32_t a0_true = WN * WN;
//     const float32_t b0_true = a0_true;

//     printf(" -> Ground Truth H(s) params (fc=%.1f Hz):\r\n", F_CUTOFF_HZ);
//     printf("    a1_true = %e\r\n", a1_true);
//     printf("    a0_true = %e\r\n", a0_true);
//     printf("    b0_true = %e\r\n\r\n", b0_true);

//     // --- **修改點**: 調整掃頻範圍以匹配新的截止頻率 ---
//     const float32_t f_start = 20.0f;
//     const float32_t f_step = 20.0f;

//     for (int i = 0; i < NUM_FREQ_POINTS; i++)
//     {
//         float32_t f_hz = f_start + i * f_step;
//         float32_t w = 2.0f * PI * f_hz;
//         w_rad_array[i] = w;

//         float32_t den_real = a0_true - (w * w);
//         float32_t den_imag = a1_true * w;
//         float32_t den_mag_sq = den_real * den_real + den_imag * den_imag;

//         h_cmplx_array[i * 2]     = (b0_true * den_real) / den_mag_sq;
//         h_cmplx_array[i * 2 + 1] = -(b0_true * den_imag) / den_mag_sq;
//     }
// }



/**
  * @brief  使用雙線性變換將 H(s) 係數轉換為 H(z) 係數
  * @param  tf_s: 指向包含 H(s) 係數的結構體的指標
  * @param  Ts: 採樣週期 (1 / Fs)
  * @param  coeffs_z: 指向長度為5的浮點數陣列的指標，用於存放 H(z) 係數 [b0,b1,b2,-a1,-a2]
  */
static void discretize_tf_bilinear(const ContinuousTransferFunction* tf_s, float32_t Ts, float32_t* coeffs_z)
{
    float32_t b0s = tf_s->b0;
    float32_t b1s = tf_s->b1;
    float32_t b2s = tf_s->b2;
    float32_t a0s = tf_s->a0;
    float32_t a1s = tf_s->a1;

    float32_t K = 2.0f / Ts;
    float32_t K2 = K * K;

    // 分母的公因數 A
    float32_t A = K2 + a1s * K + a0s;
    if (fabsf(A) < 1e-9f) { // 避免除以零
        A = 1e-9f;
    }
    float32_t A_inv = 1.0f / A;

    // 離散係數
    float32_t b0z = (b2s * K2 + b1s * K + b0s) * A_inv;
    float32_t b1z = (2.0f * b0s - 2.0f * b2s * K2) * A_inv;
    float32_t b2z = (b2s * K2 - b1s * K + b0s) * A_inv;
    float32_t a1z = (2.0f * a0s - 2.0f * K2) * A_inv;
    float32_t a2z = (K2 - a1s * K + a0s) * A_inv;

    // 填充到CMSIS-DSP格式的陣列中
    coeffs_z[0] = b0z;
    coeffs_z[1] = b1z;
    coeffs_z[2] = b2z;
    coeffs_z[3] = -a1z; // CMSIS-DSP 使用 -a1, -a2
    coeffs_z[4] = -a2z;
}


static void run_full_filter_test_suite(void)
{
    printf("\033[2J\033[H"); // 清除終端螢幕
    printf("====================================================\r\n");
    printf(" Starting Full Filter Identification Test Suite\r\n");
    printf("====================================================\r\n\r\n");

    test_lpf();
    test_hpf();
    test_bpf();
    test_bsf();
}


// --- 四種濾波器的測試函式 (已更新) ---
static void test_lpf(void)
{
    printf("\r\n\r\n--- [1/4] TESTING LOW-PASS FILTER (LPF) ---\r\n");
    static float32_t w_rad[NUM_FREQ_POINTS];
    static float32_t H_measured_cmplx[NUM_FREQ_POINTS * 2];
    const float32_t FC = 250.0f;

    generate_simulated_lpf_data(w_rad, H_measured_cmplx, FC);
    identify_filter(&identified_tf, w_rad, H_measured_cmplx);
    const float32_t Ts = 1.0f / SYSTEM_SAMPLING_RATE_HZ;
    discretize_tf_bilinear(&identified_tf, Ts, iir_coeffs);
    arm_biquad_cascade_df1_init_f32(&iir_filter_instance, 1, iir_coeffs, iir_state);
    
    // **修改**: 傳遞 ZETA (0.7071f) 作為 q_factor
    run_time_domain_simulation("LPF", FC, 0.7071f);
}

static void test_hpf(void)
{
    printf("\r\n\r\n--- [2/4] TESTING HIGH-PASS FILTER (HPF) ---\r\n");
    static float32_t w_rad[NUM_FREQ_POINTS];
    static float32_t H_measured_cmplx[NUM_FREQ_POINTS * 2];
    const float32_t FC = 500.0f;

    generate_simulated_hpf_data(w_rad, H_measured_cmplx, FC);
    identify_filter(&identified_tf, w_rad, H_measured_cmplx);
    const float32_t Ts = 1.0f / SYSTEM_SAMPLING_RATE_HZ;
    discretize_tf_bilinear(&identified_tf, Ts, iir_coeffs);
    arm_biquad_cascade_df1_init_f32(&iir_filter_instance, 1, iir_coeffs, iir_state);
    
    run_time_domain_simulation("HPF", FC, 0.7071f);
}

static void test_bpf(void)
{
    printf("\r\n\r\n--- [3/4] TESTING BAND-PASS FILTER (BPF) ---\r\n");
    static float32_t w_rad[NUM_FREQ_POINTS];
    static float32_t H_measured_cmplx[NUM_FREQ_POINTS * 2];
    const float32_t F0 = 1000.0f;
    const float32_t Q = 5.0f;

    generate_simulated_bpf_data(w_rad, H_measured_cmplx, F0, Q);
    identify_filter(&identified_tf, w_rad, H_measured_cmplx);
    const float32_t Ts = 1.0f / SYSTEM_SAMPLING_RATE_HZ;
    discretize_tf_bilinear(&identified_tf, Ts, iir_coeffs);
    arm_biquad_cascade_df1_init_f32(&iir_filter_instance, 1, iir_coeffs, iir_state);
    
    run_time_domain_simulation("BPF", F0, Q);
}

static void test_bsf(void)
{
    printf("\r\n\r\n--- [4/4] TESTING BAND-STOP FILTER (BSF) ---\r\n");
    static float32_t w_rad[NUM_FREQ_POINTS];
    static float32_t H_measured_cmplx[NUM_FREQ_POINTS * 2];
    const float32_t F0 = 1500.0f;
    const float32_t Q = 10.0f;

    generate_simulated_bsf_data(w_rad, H_measured_cmplx, F0, Q);
    identify_filter(&identified_tf, w_rad, H_measured_cmplx);
    const float32_t Ts = 1.0f / SYSTEM_SAMPLING_RATE_HZ;
    discretize_tf_bilinear(&identified_tf, Ts, iir_coeffs);
    arm_biquad_cascade_df1_init_f32(&iir_filter_instance, 1, iir_coeffs, iir_state);
    
    run_time_domain_simulation("BSF", F0, Q);
}


/**
 * @brief **核心修改**: 執行穩態時域模擬，對比理論與實際輸出
 * @param filter_type 濾波器類型
 * @param f_char 特徵頻率 (Hz)
 * @param q_factor 品質因子 (對 LPF/HPF 則為 ZETA)
 */
static void run_time_domain_simulation(const char* filter_type, float32_t f_char, float32_t q_factor)
{
    printf("STEP 4: Verifying with Steady-State Waveform. Check Vofa+.\r\n");
    printf(" -> Channels: 1.Input(blue), 2.Identified(red), 3.Theoretical(green)\r\n");
    
    float32_t test_freqs[3];
    test_freqs[0] = f_char * 0.2f;
    test_freqs[1] = f_char;
    test_freqs[2] = f_char * 5.0f;
    
    if (strcmp(filter_type, "BSF") == 0) {
        test_freqs[0] = f_char * 5.0f;
        test_freqs[2] = f_char * 0.2f;
    }

    const float32_t Ts = 1.0f / SYSTEM_SAMPLING_RATE_HZ;
    const uint32_t SETTLING_POINTS = 200; // 等待穩態的點數
    const uint32_t ANALYSIS_POINTS = 400; // 用於分析和繪圖的點數

    for (int f_idx = 0; f_idx < 3; f_idx++)
    {
        float32_t freq = test_freqs[f_idx];
        printf(" -> Simulating %s with %.1f Hz sine wave...\r\n", filter_type, freq);
        
        // 1. 計算理論響應的增益和相位
        float32_t mag_truth, phase_truth_rad;
        calculate_analog_response(filter_type, freq, f_char, q_factor, &mag_truth, &phase_truth_rad);
        
        // 2. 重置濾波器狀態
        memset(iir_state, 0, sizeof(iir_state));

        float32_t sum_sq_error = 0.0f;

        // 3. 執行模擬
        for(uint32_t i = 0; i < SETTLING_POINTS + ANALYSIS_POINTS; i++)
        {
            float32_t t = i * Ts;
            float32_t input_signal = sinf(2.0f * PI * freq * t);
            float32_t output_identified = 0.0f;
            
            // 得到辨識濾波器的輸出
            arm_biquad_cascade_df1_f32(&iir_filter_instance, &input_signal, &output_identified, 1);
            
            // 計算理論濾波器的輸出
            float32_t output_theoretical = mag_truth * sinf(2.0f * PI * freq * t + phase_truth_rad);

            // 4. 進入穩態後，累加誤差
            if (i >= SETTLING_POINTS)
            {
                float32_t error = output_theoretical - output_identified;
                sum_sq_error += error * error;
            }
            
            // 5. 輸出三通道波形到 Vofa+
            vofa_output_wave_3ch(input_signal, output_identified, output_theoretical);
            
            HAL_Delay(1); 
        }

        // 6. 計算並打印 RMS 誤差
        float32_t rms_error = sqrtf(sum_sq_error / ANALYSIS_POINTS);
        printf(" -> Steady-State RMS Error: %e\r\n", rms_error);
        
        HAL_Delay(500);
    }
}

/**
 * @brief **新增**: 計算類比濾波器在特定頻率下的理論增益和相位
 */
static void calculate_analog_response(const char* type, float32_t f, float32_t f_char, float32_t q, float32_t* mag, float32_t* phase_rad)
{
    float32_t w = 2.0f * PI * f;
    float32_t w0 = 2.0f * PI * f_char;
    float32_t a1, a0;
    float32_t num_re = 0, num_im = 0, den_re = 0, den_im = 0;

    if (strcmp(type, "LPF") == 0 || strcmp(type, "HPF") == 0) { // LPF/HPF
        a1 = 2.0f * q * w0; // q is ZETA for LPF/HPF
        a0 = w0 * w0;
        den_re = a0 - w*w;
        den_im = a1 * w;
        if (strcmp(type, "LPF") == 0) {
            num_re = a0;
        } else { // HPF
            num_re = -w*w;
        }
    } else { // BPF/BSF
        a1 = w0 / q; // q is Q-factor for BPF/BSF
        a0 = w0 * w0;
        den_re = a0 - w*w;
        den_im = a1 * w;
        if (strcmp(type, "BPF") == 0) {
            num_im = a1 * w;
        } else { // BSF
            num_re = a0 - w*w;
        }
    }

    float32_t complex_out_re = (num_re * den_re + num_im * den_im) / (den_re*den_re + den_im*den_im);
    float32_t complex_out_im = (num_im * den_re - num_re * den_im) / (den_re*den_re + den_im*den_im);
    
    *mag = sqrtf(complex_out_re*complex_out_re + complex_out_im*complex_out_im);
    *phase_rad = atan2f(complex_out_im, complex_out_re);
}


/**
 * @brief **修改**: 將三通道波形數據發送到Vofa+
 */
static void vofa_output_wave_3ch(float32_t ch1, float32_t ch2, float32_t ch3)
{
    // 格式: float,float,float\n
    printf("%.4f,%.4f,%.4f\n", ch1, ch2, ch3);
}


// =========================================================================
// 數據生成器 (為每種類型建立一個)
// =========================================================================

// LPF: H(s) = w_n^2 / (s^2 + 2*zeta*w_n*s + w_n^2)
static void generate_simulated_lpf_data(float32_t* w_rad_array, float32_t* h_cmplx_array, float32_t fc)
{
    printf(" -> Simulating LPF with fc = %.1f Hz\r\n", fc);
    const float32_t WN = 2.0f * PI * fc;
    const float32_t ZETA = 0.7071f; // Butterworth
    const float32_t a1 = 2.0f * ZETA * WN;
    const float32_t a0 = WN * WN;
    const float32_t b0 = a0;

    const float32_t f_start = 20.0f, f_step = 20.0f;
    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        float32_t w = 2.0f * PI * (f_start + i * f_step);
        w_rad_array[i] = w;
        float32_t den_real = a0 - (w * w);
        float32_t den_imag = a1 * w;
        float32_t den_mag_sq = den_real * den_real + den_imag * den_imag;
        h_cmplx_array[i * 2]     = (b0 * den_real) / den_mag_sq;
        h_cmplx_array[i * 2 + 1] = -(b0 * den_imag) / den_mag_sq;
    }
}

// HPF: H(s) = s^2 / (s^2 + 2*zeta*w_n*s + w_n^2)
static void generate_simulated_hpf_data(float32_t* w_rad_array, float32_t* h_cmplx_array, float32_t fc)
{
    printf(" -> Simulating HPF with fc = %.1f Hz\r\n", fc);
    const float32_t WN = 2.0f * PI * fc;
    const float32_t ZETA = 0.7071f; // Butterworth
    const float32_t a1 = 2.0f * ZETA * WN;
    const float32_t a0 = WN * WN;

    const float32_t f_start = 20.0f, f_step = 20.0f;
    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        float32_t w = 2.0f * PI * (f_start + i * f_step);
        w_rad_array[i] = w;
        float32_t w_sq = w * w;
        float32_t den_real = a0 - w_sq;
        float32_t den_imag = a1 * w;
        float32_t den_mag_sq = den_real * den_real + den_imag * den_imag;
        // H(jw) = -w^2 / ((a0-w^2) + j*a1*w)
        h_cmplx_array[i * 2]     = (-w_sq * den_real) / den_mag_sq;
        h_cmplx_array[i * 2 + 1] = -(-w_sq * den_imag) / den_mag_sq;
    }
}

// BPF: H(s) = (w0/Q)*s / (s^2 + (w0/Q)*s + w0^2)
static void generate_simulated_bpf_data(float32_t* w_rad_array, float32_t* h_cmplx_array, float32_t f0, float32_t q)
{
    printf(" -> Simulating BPF with f0 = %.1f Hz, Q = %.1f\r\n", f0, q);
    const float32_t W0 = 2.0f * PI * f0;
    const float32_t a1 = W0 / q;
    const float32_t a0 = W0 * W0;
    const float32_t b1 = a1;

    const float32_t f_start = 20.0f, f_step = 20.0f;
    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        float32_t w = 2.0f * PI * (f_start + i * f_step);
        w_rad_array[i] = w;
        float32_t den_real = a0 - (w * w);
        float32_t den_imag = a1 * w;
        float32_t den_mag_sq = den_real * den_real + den_imag * den_imag;
        // H(jw) = (b1*j*w) / ((a0-w^2) + j*a1*w)
        h_cmplx_array[i * 2]     = (b1 * w * den_imag) / den_mag_sq;
        h_cmplx_array[i * 2 + 1] = (b1 * w * den_real) / den_mag_sq;
    }
}

// BSF: H(s) = (s^2 + w0^2) / (s^2 + (w0/Q)*s + w0^2)
static void generate_simulated_bsf_data(float32_t* w_rad_array, float32_t* h_cmplx_array, float32_t f0, float32_t q)
{
    printf(" -> Simulating BSF with f0 = %.1f Hz, Q = %.1f\r\n", f0, q);
    const float32_t W0 = 2.0f * PI * f0;
    const float32_t W0_SQ = W0 * W0;
    const float32_t a1 = W0 / q;
    const float32_t a0 = W0_SQ;
    const float32_t b0 = a0;

    const float32_t f_start = 20.0f, f_step = 20.0f;
    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        float32_t w = 2.0f * PI * (f_start + i * f_step);
        w_rad_array[i] = w;
        float32_t w_sq = w * w;
        float32_t num_real = b0 - w_sq;
        float32_t den_real = a0 - w_sq;
        float32_t den_imag = a1 * w;
        float32_t den_mag_sq = den_real * den_real + den_imag * den_imag;
        // H(jw) = (b0 - w^2) / ((a0-w^2) + j*a1*w)
        h_cmplx_array[i * 2]     = (num_real * den_real) / den_mag_sq;
        h_cmplx_array[i * 2 + 1] = -(num_real * den_imag) / den_mag_sq;
    }
}


/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
//#include "cmsis_os.h"
#include "cmsis_os2.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "ktv.h"
#include "version.h"
#include "string.h"
    
#include "global_types.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
//RTC_HandleTypeDef hrtc;
//RTC_TimeTypeDef sTime = {0};
//RTC_DateTypeDef sDate = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);

//static void MX_RTC_Init(void);

void cutout(uint8_t iPin1, uint8_t iPin2, uint8_t iPin3, uint8_t iPin4);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// memory for freertos
uint8_t ucHeap[configTOTAL_HEAP_SIZE] @ ".ccmram" = {0};
//uint8_t ucHeap[configTOTAL_HEAP_SIZE] = {0};

extern const uint16_t buf_size;

uint8_t gPos = 0;
uint8_t gInd = 0;
uint16_t gBufInx = 0;

#pragma location = (0x080FFFFF - 3 - 4)
__root const uint32_t firmware_version = FW_VERSION;

#pragma location = (0x080FFFFF - 3 - 4 - 20)
__root const char date_time_build[20] = { BUILD_DATE };
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {

#ifdef DEBUG
  //__set_PRIMASK(1);
  //SCB->VTOR = 0x08020000;
  //__set_PRIMASK(0);
  
  // configure stack pointer
  //__set_SP(0x08020000);
#endif

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
  
  MX_GPIO_Init();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  
  MX_DMA_Init();
  MX_SPI4_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM12_Init();
  MX_TIM14_Init();

#ifdef FAST_TICK
  MX_TIM11_Init();
#endif
  
  //MX_RTC_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */


  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

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
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 216;  
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);
}

/* USER CODE BEGIN 4 */

void cutout(uint8_t iPin1, uint8_t iPin2, uint8_t iPin3, uint8_t iPin4) {
  gPos = gBufInx / 64;
  gInd = gBufInx % 64;
  
  if (iPin1) {
    opto1[gPos] |= (1 << gInd);
  } else {
    opto1[gPos] &= ~(1 << gInd);
  }
  if (iPin2) {
    opto2[gPos] |= (1 << gInd);
  } else {
    opto2[gPos] &= ~(1 << gInd);
  }
  
  if (iPin3) {
    opto3[gPos] |= (1 << gInd);
  } else {
    opto3[gPos] &= ~(1 << gInd);
  }
  if (iPin4) {
    opto4[gPos] |= (1 << gInd);
  } else {
    opto4[gPos] &= ~(1 << gInd);
  }
  gBufInx++;
  
  if (gBufInx >= buf_size) {
    gBufInx = 0;
    checkline = false;
  }
}

/* USER CODE END 4 */

#ifdef FAST_TICK
volatile uint64_t fast_tick = 0;
#endif

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
#ifdef FAST_TICK
  if (htim->Instance == TIM11) {
    fast_tick++;
  }
#endif
  if (htim->Instance == TIM14) {
    GPIO_PinState state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0);
    uint8_t val = (state == GPIO_PIN_SET) ? 0 : 1;
    KTV_SetTickValue(val);
  }
  
  if (htim->Instance == TIM12 && checkline == true) {
/*
    if (checkline == false) {
      return;
    }
*/
    GPIO_PinState state1 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6);
    GPIO_PinState state2 = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_15);
    GPIO_PinState state3 = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_10);
    GPIO_PinState state4 = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11);
    
    cutout(state1, state2, state3, state4);
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
/*
static void MX_RTC_Init(void) {
  // Initialize RTC Only
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 249;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) {
    Error_Handler();
  }

  // Initialize RTC and set the Time and Date
  sTime.Hours = 0;
  sTime.Minutes = 0;
  sTime.Seconds = 0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 1;
  sDate.Year = 0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
    Error_Handler();
  }
  
  // Enable Direct Read of the calendar registers (not through Shadow registers)
  HAL_RTCEx_EnableBypassShadow(&hrtc);
}
*/

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
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

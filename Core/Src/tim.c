/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "tim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

TIM_HandleTypeDef htim11;
TIM_HandleTypeDef htim12;
TIM_HandleTypeDef htim14;

void MX_TIM11_Init(void) {
  htim11.Instance = TIM11;
  htim11.Init.Prescaler = 0;
  htim11.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim11.Init.Period = 900;
  htim11.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim11) != HAL_OK)
  {
    Error_Handler();
  }
}

/* TIM12 init function */
void MX_TIM12_Init(void)
{

  /* USER CODE BEGIN TIM12_Init 0 */

  /* USER CODE END TIM12_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};

  /* USER CODE BEGIN TIM12_Init 1 */

  /* USER CODE END TIM12_Init 1 */
  htim12.Instance = TIM12;
  htim12.Init.Prescaler = 179;
  htim12.Init.CounterMode = TIM_COUNTERMODE_UP;
  //htim12.Init.Period = 399; // 0.8 ms
  htim12.Init.Period = 199; // 0.4 ms
  htim12.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim12.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim12) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim12, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM12_Init 2 */

  /* USER CODE END TIM12_Init 2 */

}
/* TIM14 init function */
void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 179;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 999; // 2.0 ms
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle) {
  if (tim_baseHandle->Instance == TIM11) {
    // TIM11 clock enable
    __HAL_RCC_TIM11_CLK_ENABLE();
    // TIM11 interrupt Init
    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
  }
  else if(tim_baseHandle->Instance==TIM12)
  {
    /* USER CODE BEGIN TIM12_MspInit 0 */
    
    /* USER CODE END TIM12_MspInit 0 */
    /* TIM12 clock enable */
    __HAL_RCC_TIM12_CLK_ENABLE();
    
    /* TIM12 interrupt Init */
    HAL_NVIC_SetPriority(TIM8_BRK_TIM12_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM8_BRK_TIM12_IRQn);
    /* USER CODE BEGIN TIM12_MspInit 1 */
    
    /* USER CODE END TIM12_MspInit 1 */
  }
  else if(tim_baseHandle->Instance==TIM14)
  {
    /* USER CODE BEGIN TIM14_MspInit 0 */
    
    /* USER CODE END TIM14_MspInit 0 */
    /* TIM14 clock enable */
    __HAL_RCC_TIM14_CLK_ENABLE();
    
    /* TIM14 interrupt Init */
    HAL_NVIC_SetPriority(TIM8_TRG_COM_TIM14_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM8_TRG_COM_TIM14_IRQn);
    /* USER CODE BEGIN TIM14_MspInit 1 */
    
    /* USER CODE END TIM14_MspInit 1 */
  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle) {
  if (tim_baseHandle->Instance == TIM11) {
    // Peripheral clock disable
    __HAL_RCC_TIM11_CLK_DISABLE();    
    // TIM10 interrupt Deinit
    HAL_NVIC_DisableIRQ(TIM1_TRG_COM_TIM11_IRQn);
  }
  else if(tim_baseHandle->Instance==TIM12)
  {
    /* USER CODE BEGIN TIM12_MspDeInit 0 */
    
    /* USER CODE END TIM12_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM12_CLK_DISABLE();
    
    /* TIM12 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM8_BRK_TIM12_IRQn);
    /* USER CODE BEGIN TIM12_MspDeInit 1 */
    
    /* USER CODE END TIM12_MspDeInit 1 */
  }
  else if(tim_baseHandle->Instance==TIM14)
  {
    /* USER CODE BEGIN TIM14_MspDeInit 0 */
    
    /* USER CODE END TIM14_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM14_CLK_DISABLE();
    
    /* TIM14 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM8_TRG_COM_TIM14_IRQn);
    /* USER CODE BEGIN TIM14_MspDeInit 1 */
    
    /* USER CODE END TIM14_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void Start_IT_TIM11() {
  HAL_TIM_Base_Start_IT(&htim11);
}

void Start_IT_TIM12() {
  HAL_TIM_Base_Start_IT(&htim12);
}
  
void Start_IT_TIM14() {
  HAL_TIM_Base_Start_IT(&htim14);
}
/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

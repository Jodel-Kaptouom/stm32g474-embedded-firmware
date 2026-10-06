/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
 volatile uint32_t background_counter = 0;
 volatile uint32_t button_press_count = 0;
 volatile uint32_t ms_ticks = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void My_SysTick_Init_1ms(void);
void my_delay_ms(uint32_t delay);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  My_SysTick_Init_1ms();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  // Activer l'horloge du Port A, C et SYSCFG
    RCC->AHB2ENR |= (1U << 0);
    RCC->AHB2ENR |= (1U << 2);
    RCC-> APB2ENR |= (1U<<0);

   // 2. Configurer PA5 en sortie (bits 11:10 = 01)
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (1U << (5 * 2));
   // 2. Configurer PC13 en entree (bits 26:27 = 00)
    GPIOC->MODER &= ~(3U << (13 * 2));

    // Aiguille la ligne 13 sur le Port C
    SYSCFG-> EXTICR[3] &= ~(0x0FU << 4);  //Initialiser a 0
    SYSCFG-> EXTICR[3] |= (2U << 4);

    // Active la détection front montant sur la ligne 13
    EXTI->RTSR1 |= (1U << 13);
    // Démasque l'interruption sur la ligne 13
    EXTI->IMR1 |= (1U << 13);
    // Active l'interruption dans le NVIC
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    uint32_t last_toggle_time = 0;
  while (1)
  {
    /* USER CODE END WHILE */

		  if (ms_ticks - last_toggle_time >= 500)
		  {
			  last_toggle_time = ms_ticks;
			  GPIOA->ODR ^= (1U << 5);
		  }
	  //background_counter++;
	  //if ((background_counter % 500000) ==0)
	  //{
		//  GPIOA->BSRR ^= (1U << 5);
	  //}
	/* USER CODE BEGIN 3
	  GPIOA->BSRR |= (1U << 5);
	  my_delay_ms(500);

	  GPIOA->BSRR |= (1U << (5+16));
	  my_delay_ms(500);
    */
  }
  /* USER CODE END 3 */
}

void EXTI15_10_IRQHandler (void)
{
	// 1. Vérifier si c'est bien la ligne 13 qui a provoqué l'interruption
	if (EXTI->PR1 & (1U << 13))
	{
		// 2. OBLIGATOIRE : Effacer le bit pour acquitter (Write 1 to clear)
		EXTI->PR1 |= (1U << 13);
		//// 3. Action : Inverser l'état de la LED verte (PA5)
		GPIOA->ODR ^= (1U << 5);
		button_press_count++;
	}
}

void My_SysTick_Init_1ms (void)
{
	SysTick->LOAD = (16000000/1000) - 1; // 15999
	SysTick->VAL = 0;   			// Remet le compteur à zéro
	SysTick->CTRL |= (7U << 0);		// 0x07: Core Clock + Interrupt + Enable
}

/*
void SysTick_Handler (void)
{
	ms_ticks++;			// Chaque passage ici = exactement 1 ms écoulée
}

void my_delay_ms(uint32_t delay)
{
	uint32_t start = ms_ticks;
	while ((ms_ticks - start) < delay)
	{
		// On attend que les interruptions SysTick fassent monter ms_ticks
	}
}
*/
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
#ifdef USE_FULL_ASSERT
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

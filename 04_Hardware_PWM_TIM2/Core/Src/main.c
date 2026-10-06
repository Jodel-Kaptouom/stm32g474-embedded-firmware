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
void TIM2_PWM_Init(void);
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
  TIM2_PWM_Init();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

    /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    //uint32_t last_toggle_time = 0;
    int16_t duty = 0;
    int8_t step = 5;
  while (1)
  {
    /* USER CODE END WHILE */

	  duty += step;
      if (duty >= 999) { duty = 999; step = -5; }
      if (duty <= 0)   { duty = 0;   step = 5;  }

      TIM2->CCR1 = duty; // Mise à jour directe du registre comparateur
      my_delay_ms(5);    // Attente fluide
	/* USER CODE BEGIN 3
	  GPIOA->BSRR |= (1U << 5);
	  my_delay_ms(500);

	  GPIOA->BSRR |= (1U << (5+16));
	  my_delay_ms(500);
    */
  }
  /* USER CODE END 3 */
}


void My_SysTick_Init_1ms (void)
{
	SysTick->LOAD = (16000000/1000) - 1; // 15999
	SysTick->VAL = 0;   			// Remet le compteur à zéro
	SysTick->CTRL |= (7U << 0);		// 0x07: Core Clock + Interrupt + Enable
}
void TIM2_PWM_Init(void)
{
	/* 1. Activer les horloges (GPIOA sur AHB2, TIM2 sur APB1) */
	RCC->AHB2ENR |= (1U << 0);  // GPIOA
	RCC->APB1ENR1 |= (1U << 0); // TIM2
	/* 2. Configurer PA5 en Alternate Function AF1 (TIM2_CH1) */
	GPIOA->MODER &= ~(3U << (5*2));
	GPIOA->MODER |= (2U << (5*2));		// Mode AF (10) alternativ function

	GPIOA->AFR[0] &= ~(0xFU << (5*4));
	GPIOA->AFR[0] |= (1U << (5*4));
	/* 3. Base de temps : 1 kHz */
	TIM2->PSC = 15;						// Fréquence compteur = 1 MHz
	TIM2->ARR = 999;					// Période PWM = 1 ms (1 kHz)
	TIM2->CCR1 = 250;					// Rapport cyclique initial = 25%
	/* 4. Configuration du Canal 1 en mode PWM 1 */
	TIM2->CCMR1 &= ~(0x7U << 4); 		// Nettoie OC1M
	TIM2->CCMR1 |= (0x6U << 4);			// OC1M = 0110 (PWM mode 1)
	TIM2->CCMR1 |= (1U << 3);
	/* 5. Activer la sortie physique du canal 1 */
	TIM2->CCER |= (1U << 0);			// CC1E = 1
	/* 6. Démarrer le timer */
	TIM2->CR1 |=(1U << 0);				// CEN = 1


}

/*
void SysTick_Handler (void)
{
	ms_ticks++;			// Chaque passage ici = exactement 1 ms écoulée
}
*/
void my_delay_ms(uint32_t delay)
{
	uint32_t start = ms_ticks;
	while ((ms_ticks - start) < delay)
	{
		// On attend que les interruptions SysTick fassent monter ms_ticks
	}
}

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

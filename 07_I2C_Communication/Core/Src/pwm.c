/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "pwm.h"

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

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "uart.h"
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
void USART2_Init(void)
{
	// 1.L'horloge de GPIOA et celle de l'USART2.
	RCC->APB1ENR1 |=(1U << 17);  	// L'horloge de l'USART2
	RCC->AHB2ENR |= (1U << 0);		// L'horloge de GPIOA .
	// 2. La configuration de PA2 et PA3 en Alternate Function (mode 10 dans MODER).
	GPIOA->MODER &= ~(3U << (2*2));	//PA2 a 00
	GPIOA->MODER |= (2U << (2*2));	// PA2 en AF
	GPIOA->MODER &= ~(3U << (3*2));	//PA3 a 00
	GPIOA->MODER |= (2U << (3*2));	// PA3 en AF
	// 3. L'assignation de AF7 sur PA2 et PA3 dans AFR[0].
	GPIOA->AFR[0] &= ~(0xFU << (2*4));  //AFR[0] pout les broches de 0 a 7
	GPIOA->AFR[0] |= (0x07U << (2*4));
	GPIOA->AFR[0] &= ~(0xFU << (3*4));
	GPIOA->AFR[0] |= (0x07U << (3*4));
	// 4. Le Baud Rate dans BRR
	USART2->BRR = 139;
	// 5. L'activation de TE, RE et UE dans CR1.
	USART2->CR1 &= ~(0xFU << 0);
	USART2->CR1 |= (0xDU << 0);

}

void USART2_SendChar(char c)
{
	// Attendre que le registre TDR soit vide (TXE = bit 7)
	while (!(USART2->ISR & (1U << 7)))
	{
		// Tant quil est a 0 le TXE est plein et On attend que TXE passe à 1 pour ecrire a linterieur
	}
	// Écrire le caractère
	USART2->TDR = c;
}

void USART2_SendString (char *str)
{
	while (*str)
	{
		USART2_SendChar(*str++);
	}
}

char USART2_GetChar(void)
{
	while (!(USART2->ISR & (1U << 5)))
	{
		if (USART2->ISR & (1U << 3))		// overrun détecté ? if (USART2->ISR & USART_ISR_ORE)
		{
			 USART2->ICR = USART_ICR_ORECF; 	//// on efface l'erreur USART2->ICR |= (1U << 0);
		}
		// Tant que le bit RXNE (bit 5) du registre USART2->ISR est égal à 0.
	}
	return (char)(USART2->RDR & 0xFF);
}

int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        USART2_SendChar(*ptr++);
    }
    return len;
}

#include "adc.h"
void ADC1_Init(void)
{
	/* 1. Horloges */
	RCC->AHB2ENR |= (1U << 13);		// Adc
	RCC-> AHB2ENR |= (1U << 0);     //GPIOA

	/* 2. PA0 en mode analogique (11) */
	GPIOA->MODER |= (3U << 0);

	/* 3. Horloge synchrone pour ADC1/2 */
	ADC12_COMMON->CCR &= ~(3U << 16);
	ADC12_COMMON->CCR |= (1U << 16); 	// CKMODE = 01 (HCLK / 1)

	/* 4. Sortir du mode Deep-Power-Down (DEEPPWD = 0) */
	ADC1->CR &= ~(1U << 29);
	/* 5. Activer le régulateur interne de l'ADC (ADVREGEN = 1) */
	ADC1->CR |= (1U << 28);

	/* 6. Attendre la stabilisation du régulateur (t_STAB ≈ 20 µs) */
	for (volatile int i=0; i< 1000; i++);

	/* 7. Étalonnage automatique (Calibration) */
	ADC1->CR &= ~(1U << 30); 		// ADCALDIF = 0 (Calibration Single-Ended)
	ADC1->CR |= (1U << 31);			// Lance la calibration (ADCAL = 1)
	while ((ADC1->CR & (1U << 31)));		 // ADCAL repasse à 0 quand c'est fini

	/* 8. Activer l'ADC */
	ADC1->ISR = (1U << 0);               // efface ADRDY (en écrivant 1)
	ADC1->CR |= (1U << 0);               // ADEN = 1
	while (!(ADC1->ISR & (1U << 0)));    // attend ADRDY = 1

	/* 9. Configurer la séquence : 1 seule conversion sur le Canal 1 */
	ADC1->SQR1 &= ~(0xFU << 0);          // L[3:0] = 0 (longueur = 1 conversion)
	ADC1->SQR1 &= ~(0x1FU << 6);
	ADC1->SQR1 |=  (1U << 6);            // SQ1 = Canal 1 (PA0)
}

uint16_t ADC1_read (void)
{
	ADC1->CR |= (1U << 2);			    // start conversion with the bit 2 of the CR
	while (!(ADC1->ISR & (1U << 2)));	// wait until the conversion is at the end  EOC = 1 (registre ISR)
	return (uint16_t)ADC1->DR;			// Lire le résultat (efface automatiquement EOC)
}

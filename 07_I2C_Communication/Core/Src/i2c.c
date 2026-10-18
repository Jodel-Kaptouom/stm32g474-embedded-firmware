#include "i2c.h"
/* Environ 50 ms à 16 MHz : largement plus qu'un octet à 100 kHz (~0,2 ms) */
#define I2C_TIMEOUT  100000U

void I2C1_Init (void)
{
	/* 1. Horloges */
	RCC->APB1ENR1 |= (1U << 21);		// i2c
	RCC->AHB2ENR |= (1U << 1);		// GPIOB

	/* 2. PB8 und PB9 en mode AF */
	GPIOB->MODER &= ~(3U << (8*2));
	GPIOB->MODER &= ~(3U << (9*2));
	GPIOB->MODER |= (2U << (8*2));
	GPIOB->MODER |= (2U << (9*2));

	/* 3. Configurer PB8 et PB9 en Open-Drain (obligatoire en I2C) */
	GPIOB->OTYPER |= (1U << 8) | (1U << 9);

	/* 4. Activer les résistances de Pull-up internes (sécurité bus libre) */
	GPIOB->PUPDR &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
	GPIOB->PUPDR |=  ((1U << (8 * 2)) | (1U << (9 * 2))); // 01 = Pull-up

	/* 5. Assigner AF4 (I2C1) dans AFR[1] (AFRH) */
	// PB8 -> AF4 (décalage: (8 - 8) * 4 = 0)
	GPIOB-> AFR[1] &= ~(0xFU << 0);
	GPIOB-> AFR[1] |= (4U << 0);
	// PB9 -> AF4 (décalage: (9 - 8) * 4 = 4)
	GPIOB-> AFR[1] &= ~(0xFU << 4);
	GPIOB-> AFR[1] |= (4U << 4);

	/* 6. Désactiver I2C1 pendant la configuration des registres */
	I2C1->CR1 &= ~(1U << 0); // PE = 0


	/* 7. Régler les timings pour Standard Mode 100 kHz @ 16 MHz */
	I2C1->TIMINGR = 0x00303D5B;

	/* 8. Activer le périphérique I2C */
	I2C1->CR1 |= (1U << 0);  // PE = 1
	//I2C1->CR1 = I2C1_CR1_PE
}

/* Envoie 1 octet à l'esclave. Retourne I2C_OK, I2C_ERR_NACK ou I2C_ERR_TIMEOUT. */
int I2C1_WriteByte(uint8_t slave_addr, uint8_t data)
{
	uint32_t t;

	/* 1. Attendre que le bus soit libre (BUSY, bit 15) */
	t = I2C_TIMEOUT;
	while (I2C1->ISR & (1U << 15))
	{
		if (--t == 0) return I2C_ERR_TIMEOUT;
	}

	/* 2. SADD = adresse 7 bits, RD_WRN = 0 (écriture), NBYTES = 1,
	      AUTOEND = 1 (STOP automatique), START = 1 */
	I2C1->CR2 = ((uint32_t)slave_addr << 1)
	          | (1U << 16)            // NBYTES = 1
	          | (1U << 25)            // AUTOEND
	          | (1U << 13);           // START

	/* 3. Attendre TXIS (bit 1), ou un NACK (bit 4) si l'esclave ne répond pas */
	t = I2C_TIMEOUT;
	while (!(I2C1->ISR & (1U << 1)))
	{
		if (I2C1->ISR & (1U << 4))    // NACKF
		{
			/* Avec AUTOEND, le matériel envoie le STOP tout seul : on l'attend */
			t = I2C_TIMEOUT;
			while (!(I2C1->ISR & (1U << 5)))
			{
				if (--t == 0) break;
			}
			I2C1->ICR = (1U << 4) | (1U << 5);   // efface NACKF et STOPF
			return I2C_ERR_NACK;
		}
		if (--t == 0) return I2C_ERR_TIMEOUT;
	}

	/* 4. Écrire l'octet */
	I2C1->TXDR = data;

	/* 5. Attendre la fin du STOP (STOPF, bit 5) */
	t = I2C_TIMEOUT;
	while (!(I2C1->ISR & (1U << 5)))
	{
		if (--t == 0) return I2C_ERR_TIMEOUT;
	}

	/* 6. Effacer STOPF (ICR est en écriture seule : '=' et non '|=') */
	I2C1->ICR = (1U << 5);
	return I2C_OK;
}

/* Envoie seulement l'adresse (NBYTES = 0) pour savoir si un esclave répond.
   Rien n'est écrit dans le composant. Retourne I2C_OK si l'esclave acquitte. */
int I2C1_Probe(uint8_t slave_addr)
{
	uint32_t t;
	int ack;

	t = I2C_TIMEOUT;
	while (I2C1->ISR & (1U << 15))
	{
		if (--t == 0) return I2C_ERR_TIMEOUT;
	}

	I2C1->CR2 = ((uint32_t)slave_addr << 1)
	          | (1U << 25)            // AUTOEND, NBYTES = 0
	          | (1U << 13);           // START

	t = I2C_TIMEOUT;
	while (!(I2C1->ISR & (1U << 5)))  // STOPF
	{
		if (--t == 0) return I2C_ERR_TIMEOUT;
	}

	ack = !(I2C1->ISR & (1U << 4));   // pas de NACKF = l'esclave a répondu
	I2C1->ICR = (1U << 4) | (1U << 5);
	return ack ? I2C_OK : I2C_ERR_NACK;
}

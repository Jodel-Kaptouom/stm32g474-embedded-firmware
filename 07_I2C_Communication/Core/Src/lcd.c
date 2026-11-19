#include "lcd.h"
#include "i2c.h"

/* ---- Bits de l'octet envoyé au PCF8574 (P0..P7) ---- */
#define LCD_RS          0x01U   /* P0 : 0 = commande, 1 = caractère */
#define LCD_RW          0x02U   /* P1 : toujours 0 (écriture)        */
#define LCD_EN          0x04U   /* P2 : validation (front descendant) */
#define LCD_BL          0x08U   /* P3 : rétroéclairage               */

#define LCD_MODE_CMD    0x00U
#define LCD_MODE_DATA   LCD_RS

/* ---- Commandes HD44780 ---- */
#define LCD_CMD_CLEAR         0x01U   /* Clear Display                          */
#define LCD_CMD_ENTRY_MODE    0x06U   /* Curseur vers la droite, pas de décalage */
#define LCD_CMD_DISPLAY_OFF   0x08U   /* Écran, curseur, clignotement éteints   */
#define LCD_CMD_DISPLAY_ON    0x0CU   /* Écran allumé, curseur éteint           */
#define LCD_CMD_FUNCTION_SET  0x28U   /* 4 bits, 2 lignes, police 5x8           */
#define LCD_CMD_SET_DDRAM     0x80U   /* Placer le curseur (| adresse)          */

/* Délai défini dans main.c : ce prototype le rend connu ici */
void my_delay_ms(uint32_t delay);


/* ---- Couche 1 : une moitié d'octet + impulsion EN ---- */
static int LCD_SendNibble(uint8_t nibble, uint8_t mode)
{
	uint8_t data = (nibble & 0xF0U) | mode | LCD_BL;
	int status;

	status = I2C1_WriteByte(LCD_I2C_ADDR, data | LCD_EN);           /* EN = 1 */
	if (status != I2C_OK)
	{
		return status;
	}
	return I2C1_WriteByte(LCD_I2C_ADDR, data & (uint8_t)~LCD_EN);   /* EN = 0 */
}

/* ---- Couche 2 : un octet complet en deux moitiés ---- */
static int LCD_SendByte(uint8_t value, uint8_t mode)
{
	int status;

	status = LCD_SendNibble(value & 0xF0U, mode);                    /* moitié haute */
	if (status != I2C_OK)
	{
		return status;
	}
	return LCD_SendNibble((uint8_t)(value << 4) & 0xF0U, mode);      /* moitié basse */
}

/* ---- Couche 3 : commande ou caractère ---- */
static int LCD_Command(uint8_t cmd)
{
	return LCD_SendByte(cmd, LCD_MODE_CMD);
}

static int LCD_Data(char c)
{
	return LCD_SendByte((uint8_t)c, LCD_MODE_DATA);
}


/* ---- Couche 4 : fonctions publiques ---- */
int LCD_Init(void)
{
	int status;

	/* 1. Attendre > 40 ms après la mise sous tension */
	my_delay_ms(50);

	/* 2. Première moitié 0x3 : si l'écran ne répond pas, on s'arrête ici */
	status = LCD_SendNibble(0x30U, LCD_MODE_CMD);
	if (status != I2C_OK)
	{
		return status;
	}
	my_delay_ms(6);                         /* > 4,1 ms */

	LCD_SendNibble(0x30U, LCD_MODE_CMD);
	my_delay_ms(2);                         /* > 100 µs */

	LCD_SendNibble(0x30U, LCD_MODE_CMD);
	my_delay_ms(2);

	/* 3. Passage en mode 4 bits */
	LCD_SendNibble(0x20U, LCD_MODE_CMD);
	my_delay_ms(2);

	/* 4. Configuration avec des commandes complètes */
	LCD_Command(LCD_CMD_FUNCTION_SET);
	LCD_Command(LCD_CMD_DISPLAY_OFF);
	LCD_Clear();                            /* contient son propre délai */
	LCD_Command(LCD_CMD_ENTRY_MODE);
	return LCD_Command(LCD_CMD_DISPLAY_ON);
}

void LCD_Clear(void)
{
	LCD_Command(LCD_CMD_CLEAR);
	my_delay_ms(3);                         /* > 1,52 ms d'exécution */
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
	uint8_t addr = (row == 0U) ? 0x00U : 0x40U;   /* début de la ligne */
	addr += col;
	LCD_Command(LCD_CMD_SET_DDRAM | addr);
}

void LCD_Print(const char *str)
{
	while (*str)
	{
		LCD_Data(*str++);
	}
}

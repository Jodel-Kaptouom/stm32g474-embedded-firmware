#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/* Adresse I2C 7 bits du module PCF8574 */
#define LCD_I2C_ADDR   0x27

/* Initialise l'écran en mode 4 bits.
 * Retourne I2C_OK si l'écran répond, sinon le code d'erreur I2C. */
int  LCD_Init(void);

/* Efface l'écran et remet le curseur en haut à gauche. */
void LCD_Clear(void);

/* Place le curseur. row : 0 ou 1, col : 0 à 15. */
void LCD_SetCursor(uint8_t row, uint8_t col);

/* Affiche une chaîne à partir de la position du curseur. */
void LCD_Print(const char *str);

#endif /* LCD_H */

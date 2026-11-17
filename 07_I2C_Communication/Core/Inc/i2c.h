
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : i2c.h
  * @brief          : Header for adc.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_H
#define __I2C_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32g4xx.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */
#define LCD_ADDR  		0x27
#define I2C_OK            0
#define I2C_ERR_TIMEOUT  -1
#define I2C_ERR_NACK     -2
#define LCD_BACKLIGHT 0x08U  // Bit 3 = 1 (rétroéclairage allumé)
#define LCD_ENABLE    0x04U  // Bit 2 = 1 (broche EN)
/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void I2C1_Init(void);
int I2C1_WriteByte(uint8_t slave_addr, uint8_t data);
void I2C1_Scan(void);
void LCD_SendNibble(uint8_t nibble, uint8_t rs);
void LCD_SendByte(uint8_t val, uint8_t rs);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendChar(char c);
void LCD_SendString(const char *str);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#endif /* __I2C_H */

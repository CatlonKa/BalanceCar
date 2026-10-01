/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_W1_Pin GPIO_PIN_2
#define LED_W1_GPIO_Port GPIOE
#define BUZZER_Pin GPIO_PIN_5
#define BUZZER_GPIO_Port GPIOE
#define ADC_VBUS_Pin GPIO_PIN_2
#define ADC_VBUS_GPIO_Port GPIOC
#define PHL_Pin GPIO_PIN_0
#define PHL_GPIO_Port GPIOA
#define ENL_Pin GPIO_PIN_1
#define ENL_GPIO_Port GPIOA
#define PHR_Pin GPIO_PIN_2
#define PHR_GPIO_Port GPIOA
#define ENR_Pin GPIO_PIN_3
#define ENR_GPIO_Port GPIOA
#define TFT_SCK_Pin GPIO_PIN_5
#define TFT_SCK_GPIO_Port GPIOA
#define TFT_MOSI_Pin GPIO_PIN_7
#define TFT_MOSI_GPIO_Port GPIOA
#define TFT_RST_Pin GPIO_PIN_4
#define TFT_RST_GPIO_Port GPIOC
#define TFT_DC_Pin GPIO_PIN_5
#define TFT_DC_GPIO_Port GPIOC
#define TFT_CS_Pin GPIO_PIN_0
#define TFT_CS_GPIO_Port GPIOB
#define KEY0_Pin GPIO_PIN_7
#define KEY0_GPIO_Port GPIOE
#define ENCODER_AL_Pin GPIO_PIN_9
#define ENCODER_AL_GPIO_Port GPIOE
#define ENCODER_BL_Pin GPIO_PIN_11
#define ENCODER_BL_GPIO_Port GPIOE
#define KEY1_Pin GPIO_PIN_15
#define KEY1_GPIO_Port GPIOE
#define CR_Pin GPIO_PIN_12
#define CR_GPIO_Port GPIOB
#define WIFI_SCK_Pin GPIO_PIN_13
#define WIFI_SCK_GPIO_Port GPIOB
#define WIFI_MISO_Pin GPIO_PIN_14
#define WIFI_MISO_GPIO_Port GPIOB
#define WIFI_MOSI_Pin GPIO_PIN_15
#define WIFI_MOSI_GPIO_Port GPIOB
#define WIFI_ESP_CS_Pin GPIO_PIN_8
#define WIFI_ESP_CS_GPIO_Port GPIOD
#define WIFI_CS_Pin GPIO_PIN_9
#define WIFI_CS_GPIO_Port GPIOD
#define WIFI_IRQ_Pin GPIO_PIN_10
#define WIFI_IRQ_GPIO_Port GPIOD
#define WIFI_CE_Pin GPIO_PIN_11
#define WIFI_CE_GPIO_Port GPIOD
#define ENCODER_AR_Pin GPIO_PIN_12
#define ENCODER_AR_GPIO_Port GPIOD
#define ENCODER_BR_Pin GPIO_PIN_13
#define ENCODER_BR_GPIO_Port GPIOD
#define SIOD_Pin GPIO_PIN_14
#define SIOD_GPIO_Port GPIOD
#define SIOC_Pin GPIO_PIN_15
#define SIOC_GPIO_Port GPIOD
#define ESP_TX_Pin GPIO_PIN_6
#define ESP_TX_GPIO_Port GPIOC
#define ESP_RX_Pin GPIO_PIN_7
#define ESP_RX_GPIO_Port GPIOC
#define DI_LED_Pin GPIO_PIN_8
#define DI_LED_GPIO_Port GPIOC
#define XCLK_Pin GPIO_PIN_9
#define XCLK_GPIO_Port GPIOC
#define PWND_Pin GPIO_PIN_11
#define PWND_GPIO_Port GPIOA
#define OV_RESET_Pin GPIO_PIN_12
#define OV_RESET_GPIO_Port GPIOA
#define LED_W2_Pin GPIO_PIN_0
#define LED_W2_GPIO_Port GPIOD
#define LED3_Pin GPIO_PIN_1
#define LED3_GPIO_Port GPIOD
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOD
#define LED1_Pin GPIO_PIN_3
#define LED1_GPIO_Port GPIOD
#define IMU_INT1_Pin GPIO_PIN_5
#define IMU_INT1_GPIO_Port GPIOD
#define IMU_INT1_EXTI_IRQn EXTI9_5_IRQn
#define IMU_INT2_Pin GPIO_PIN_6
#define IMU_INT2_GPIO_Port GPIOD
#define IMU_INT2_EXTI_IRQn EXTI9_5_IRQn
#define IMU_CS_Pin GPIO_PIN_7
#define IMU_CS_GPIO_Port GPIOD
#define IMU_SCK_Pin GPIO_PIN_3
#define IMU_SCK_GPIO_Port GPIOB
#define IMU_MISO_Pin GPIO_PIN_4
#define IMU_MISO_GPIO_Port GPIOB
#define IMU_MOSI_Pin GPIO_PIN_5
#define IMU_MOSI_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

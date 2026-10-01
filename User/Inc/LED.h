#ifndef __LED_H__
#define __LED_H__

#include "main.h"
#include "tim.h"

#define Code0 35
#define Code1 70
#define CodeReset 0
#define NUM_LED 2

#define LED1(x) HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, (GPIO_PinState)(!x))
#define LED2(x) HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, (GPIO_PinState)(!x))
#define LED3(x) HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, (GPIO_PinState)(!x))
#define LED_W1(x) HAL_GPIO_WritePin(LED_W1_GPIO_Port, LED_W1_Pin, (GPIO_PinState)(!x))
#define LED_W2(x) HAL_GPIO_WritePin(LED_W2_GPIO_Port, LED_W2_Pin, (GPIO_PinState)(!x))

void WS2812_SetColor(uint8_t index, uint8_t R, uint8_t G, uint8_t B);
void WS2812_Update(uint8_t ledNum);
void DI_ALL_LED(uint8_t R, uint8_t G, uint8_t B);

#endif 
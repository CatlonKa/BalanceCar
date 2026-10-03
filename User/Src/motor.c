#include "motor.h"
#include "main.h"
#include <stdint.h>


void motor_init(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
    HAL_GPIO_WritePin(PHL_GPIO_Port, PHL_Pin, GPIO_PIN_SET);   /* 左轮前进 */
    HAL_GPIO_WritePin(PHR_GPIO_Port, PHR_Pin, GPIO_PIN_RESET); /* 右轮前进 */
}

void motor_drive(int16_t Left, int16_t Right)
{
    uint16_t pwm_l, pwm_r;

    /* 左轮 */
    if (Left >= 0)
    {
        HAL_GPIO_WritePin(PHL_GPIO_Port, PHL_Pin, GPIO_PIN_SET);
        pwm_l = (Left > 999) ? 999 : (uint16_t)Left;
    } else {
        HAL_GPIO_WritePin(PHL_GPIO_Port, PHL_Pin, GPIO_PIN_RESET);
        pwm_l = (-Left > 999) ? 999 : (uint16_t)(-Left);
    }

    /* 右轮 */
    if (right >= 0) {
        HAL_GPIO_WritePin(PHR_GPIO_Port, PHR_Pin, GPIO_PIN_RESET);
        pwm_r = (Right > 999) ? 999 : (uint16_t)Right;
    } else {
        HAL_GPIO_WritePin(PHR_GPIO_Port, PHR_Pin, GPIO_PIN_SET);
        pwm_r = (-Right > 999) ? 999 : (uint16_t)(-Right);
    }

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, pwm_l);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, pwm_r);
}
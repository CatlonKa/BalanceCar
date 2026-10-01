#include "motor.h"


void motor_init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
    HAL_GPIO_WritePin(PHL_GPIO_Port, PHL_Pin, GPIO_PIN_SET);   /* 左轮前进 */
    HAL_GPIO_WritePin(PHR_GPIO_Port, PHR_Pin, GPIO_PIN_RESET); /* 右轮前进 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);
}


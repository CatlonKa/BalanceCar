#include "Buzzer.h"

void buzzer_init(void)
{
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
    HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);
}

void buzzer_on(uint16_t frequency, uint16_t volume)
{
    volume = (volume > 100) ? 100 : volume;
    if (frequency == 0) { buzzer_off(); return; }
    uint16_t arr = 1000000 / frequency;
    uint16_t duty = volume * arr / 100;
    __HAL_TIM_SET_AUTORELOAD(&htim9, arr);
    __HAL_TIM_SET_COUNTER(&htim9, 0);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, duty);
}

void buzzer_off(void)
{
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
}
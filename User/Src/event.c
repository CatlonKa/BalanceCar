#include "event.h"

int16_t speedL = 0;
int16_t speedR = 0;

uint8_t car_run = 1;
volatile uint8_t car_state_changed = 0;

void event_init(void)
{
    HAL_TIM_Base_Start_IT(&htim5);
    HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //编码器
    if (htim->Instance == TIM6)
    {
        speedL = encoder_get_delta_left();
        speedR = encoder_get_delta_right();
    }


    else if (htim->Instance == TIM5)
    {
        key_tick();
        car_state();
    }
}

void car_state(void)
{
    if(!key0_flag)
    {
        return;
    }
    key0_flag = 0;

    if(car_run)
    {
        car_run = 0;
        HAL_TIM_Base_Stop_IT(&htim6);
    }
    else
    {
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        __HAL_TIM_SET_COUNTER(&htim4, 0);
        car_run = 1;
        HAL_TIM_Base_Start_IT(&htim6);
    }

    car_state_changed = 1;
}
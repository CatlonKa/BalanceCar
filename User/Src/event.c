#include "event.h"
#include "key.h"

int16_t speedL = 0;
int16_t speedR = 0;
uint8_t car_run = 0;

void event_init(void)
{
    HAL_TIM_Base_Start_IT(&htim5);
    /*
     * TIM6 刻意不在这里启动。
     *
     * TIM6 的作用是周期读编码器算速度，而编码器同一时刻只能有一个读者。
     * 上电时 car_run = 0，车轮归 UI 使用，所以 TIM6 保持关闭；
     * 等 KEY0 启动电机（car_state() 里）才把它打开。
     */
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
}
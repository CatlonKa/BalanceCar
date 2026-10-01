#include "key.h"
#include "tim.h"

#define KEY_NEED 4

uint8_t key0_flag = 0;
uint8_t key1_flag = 0;

void key_tick(void)
{
    static uint8_t time;
    static uint8_t key0_num;
    static uint8_t key1_num;
    static uint8_t key0_press;
    static uint8_t key1_press;
    uint8_t key0_state;
    uint8_t key1_state;

    time++;
    if(time % 7 != 0)
    {
        return;
    }

    //key0
    if(!key0_flag)
    {
        key0_state = HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin);

        if(!key0_press)
        {
            if(key0_num >= KEY_NEED)
            {
                key0_press = 1;
                key0_num = 0;
            }
            if(!key0_state)
            {
                key0_num++;
            }
            else
            {
                key0_num = 0;
            }
        }
        else
        {
            if(key0_num >= KEY_NEED)
            {
                key0_flag = 1;
                key0_press = 0;
                key0_num = 0;
            }
            if(key0_state)
            {
                key0_num++;
            }
            else
            {
                key0_num = 0;
            }
        }
    }

    //key1
    if(!key1_flag)
    {
        key1_state = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);

        if(!key1_press)
        {
            if(key1_num >= KEY_NEED)
            {
                key1_press = 1;
                key1_num = 0;
            }
            if(!key1_state)
            {
                key1_num++;
            }
            else
            {
                key1_num = 0;
            }
        }
        else
        {
            if(key1_num >= KEY_NEED)
            {
                key1_flag = 1;
                key1_press = 0;
                key1_num = 0;
            }
            if(key1_state)
            {
                key1_num++;
            }
            else
            {
                key1_num = 0;
            }
        }
    }
}
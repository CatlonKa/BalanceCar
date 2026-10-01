#include "LED.h"

static uint8_t s_color[NUM_LED][3] = {0};
static uint16_t s_dmaData[NUM_LED * 24 + 1];

void WS2812_SetColor(uint8_t index, uint8_t R, uint8_t G, uint8_t B)
{
    if (index >= NUM_LED) return;
    s_color[index][0] = R;
    s_color[index][1] = G;
    s_color[index][2] = B;
}


void WS2812_Update(uint8_t ledNum)
{
    if (ledNum > NUM_LED) ledNum = NUM_LED;

    uint16_t j = 0;
    for (uint8_t led = 0; led < ledNum; led++)
    {
        uint8_t R = s_color[led][0];
        uint8_t G = s_color[led][1];
        uint8_t B = s_color[led][2];

        for (uint8_t i = 0; i < 8; i++)
            s_dmaData[j++] = (G & (0x80 >> i)) ? Code1 : Code0;
        for (uint8_t i = 0; i < 8; i++)
            s_dmaData[j++] = (R & (0x80 >> i)) ? Code1 : Code0;
        for (uint8_t i = 0; i < 8; i++)
            s_dmaData[j++] = (B & (0x80 >> i)) ? Code1 : Code0;
    }
    s_dmaData[j] = CodeReset;

    HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_3);//先停掉上一次可能的 DMA,再重新发送
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, s_dmaData, j + 1);
}


void DI_ALL_LED(uint8_t R, uint8_t G, uint8_t B)
{
    for (uint8_t i = 0; i < NUM_LED; i++)
        WS2812_SetColor(i, R, G, B);
    WS2812_Update(NUM_LED);
}



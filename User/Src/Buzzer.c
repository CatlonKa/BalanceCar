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

/* 启动三声：频率递增。序列共 2*N 步，偶数步发声、奇数步静音 */
#define BUZZER_STARTUP_BEEP_MS 120U
#define BUZZER_STARTUP_GAP_MS   80U
#define BUZZER_STARTUP_VOLUME   40U

static const uint16_t s_startup_freq[] = { 800U, 1200U, 1600U };
#define BUZZER_STARTUP_STEPS (sizeof(s_startup_freq) / sizeof(s_startup_freq[0]) * 2U)

/* 由 TIM5 中断每 1 ms 推进。不能用 HAL_GetTick：TIM5 优先级 3 高于 SysTick 15，
   中断里 uwTick 不递增 */
static volatile uint8_t s_tone_step;
static uint16_t s_tone_left_ms;

void buzzer_startup_tone(void)
{
    s_tone_step = 0U;
    s_tone_left_ms = 0U;
}

void buzzer_tick(void)
{
    if (s_tone_step >= (uint8_t)BUZZER_STARTUP_STEPS)
    {
        return;
    }

    if (s_tone_left_ms != 0U)
    {
        s_tone_left_ms--;
        return;
    }

    if ((s_tone_step & 1U) == 0U)
    {
        buzzer_on(s_startup_freq[s_tone_step / 2U], BUZZER_STARTUP_VOLUME);
        s_tone_left_ms = BUZZER_STARTUP_BEEP_MS;
    }
    else
    {
        buzzer_off();
        s_tone_left_ms = BUZZER_STARTUP_GAP_MS;
    }
    s_tone_step++;
}
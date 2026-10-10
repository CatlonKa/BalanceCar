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

/* 三声频率，两种模式共用；序列共 2N 步，偶数步发声、奇数步静音 */
static const uint16_t s_tone_freq[] =
{
    BUZZER_TONE_FREQ1,
    BUZZER_TONE_FREQ2,
    BUZZER_TONE_FREQ3
};
#define BUZZER_TONE_STEPS (sizeof(s_tone_freq) / sizeof(s_tone_freq[0]) * 2U)

/* 由 TIM5 中断每 1 ms 推进。不能用 HAL_GetTick：TIM5 优先级 3 高于 SysTick 15，
   中断里 uwTick 不递增 */
static volatile uint8_t s_phase = (uint8_t)BUZZER_TONE_STEPS; /* 初值=空闲，不自动响 */
static uint16_t s_left_ms;      /* 本段剩余毫秒，0 = 该开新段 */
static uint16_t s_beep_ms;      /* 本声已发声毫秒，驱动包络/抖动/摆动 */

void buzzer_startup_tone(void)
{
    s_phase = 0U;
    s_left_ms = 0U;
    s_beep_ms = 0U;
    buzzer_off();
}

#if BUZZER_TONE_MODE == BUZZER_TONE_ESC

/* ❗调制率必须远慢于载波周期：ARR 每 1ms 改一次，改动量太大（换谐波）
   波形就走不完一个整周期 → 听到纯噪声。"频率叠加"靠调制边带做，不能靠切碎周期 */
#if ((BUZZER_ESC_VIB_MS % 2U) != 0U) || ((BUZZER_ESC_GROWL_MS % 2U) != 0U)
#error "VIB_MS / GROWL_MS 必须是偶数，否则三角波峰值会超过 100%"
#endif
#if (BUZZER_ESC_ATK_MS + BUZZER_ESC_REL_MS) > BUZZER_ESC_BEEP_MS
#error "ATK_MS + REL_MS 不能超过 BEEP_MS，否则包络的两段斜坡会重叠"
#endif

/* 三角波满量程 */
#define BUZZER_TRI_MAX 1000U

static uint16_t s_noise = 0xACE1U;

/* xorshift16 伪随机，给周期抖动去相关用 */
static uint16_t buzzer_noise(void)
{
    uint16_t x = s_noise;
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    s_noise = x;
    return x;
}

/* 起振/收尾包络，返回 0..100 */
static uint16_t buzzer_env(uint16_t ms)
{
    if (ms < BUZZER_ESC_ATK_MS) {
        return (uint16_t)((uint32_t)ms * 100U / BUZZER_ESC_ATK_MS);
    }
    if ((uint32_t)ms + BUZZER_ESC_REL_MS >= BUZZER_ESC_BEEP_MS) {
        return (uint16_t)((uint32_t)(BUZZER_ESC_BEEP_MS - ms) * 100U / BUZZER_ESC_REL_MS);
    }
    return 100U;
}

/* 三角波 0..BUZZER_TRI_MAX，周期必须偶数 */
static uint16_t buzzer_tri(uint16_t ms, uint16_t period)
{
    const uint16_t half = period / 2U;
    const uint16_t p = ms % period;
    const uint16_t t = (p < half) ? p : (uint16_t)(period - p);

    return (uint16_t)((uint32_t)t * BUZZER_TRI_MAX / half);
}

/* 合成 1ms 音频切片：颤音改频率、占空比摆动改音色，再乘包络。
   颤音与抖动都直接在 ARR 上叠加（不重算频率除法），分辨率才够细 */
static void buzzer_spit(uint8_t index, uint16_t ms)
{
    const int32_t arr0 = (int32_t)(1000000U / s_tone_freq[index]);
    const int32_t vib  = 2 * (int32_t)buzzer_tri(ms, BUZZER_ESC_VIB_MS) - (int32_t)BUZZER_TRI_MAX;
    const int32_t jit  = (int32_t)(buzzer_noise() & 0xFFU) - 128;   /* -128..127 */
    int32_t  arr = arr0
                 + arr0 * (int32_t)BUZZER_ESC_VIB_PCT * vib / (100 * (int32_t)BUZZER_TRI_MAX)
                 + arr0 * jit * (int32_t)BUZZER_ESC_JIT_PCT / (128 * 100);
    uint32_t duty_pct;
    uint32_t want;

    if (arr < 16) { arr = 16; }        /* ARR 绝不能是 0，否则定时器停摆 */

    duty_pct = BUZZER_ESC_DUTY_MIN
             + (uint32_t)(BUZZER_ESC_DUTY_MAX - BUZZER_ESC_DUTY_MIN) * buzzer_tri(ms, BUZZER_ESC_GROWL_MS) / BUZZER_TRI_MAX;

    want = (uint32_t)arr * duty_pct / 100U * BUZZER_TONE_VOLUME / 100U;
    want = want * buzzer_env(ms) / 100U;

    /* 不碰 CNT：ARPE/OC1PE 已开，新值在下一个周期边界生效，无毛刺。
       若像 buzzer_on() 那样每毫秒复位计数，周期会被切碎成直流 */
    __HAL_TIM_SET_AUTORELOAD(&htim9, (uint16_t)arr);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (uint16_t)want);
}

void buzzer_tick(void)
{
    if (s_phase >= (uint8_t)BUZZER_TONE_STEPS)
    {
        return;
    }

    if (s_left_ms == 0U)               /* 开新段 */
    {
        s_left_ms = ((s_phase & 1U) == 0U) ? BUZZER_ESC_BEEP_MS : BUZZER_ESC_GAP_MS;
        if ((s_phase & 1U) != 0U)
        {
            buzzer_off();
        }
    }

    s_left_ms--;

    if ((s_phase & 1U) == 0U)          /* 发声段：每毫秒重算一次 */
    {
        buzzer_spit((uint8_t)(s_phase >> 1U), s_beep_ms);
        s_beep_ms++;
    }

    if (s_left_ms == 0U)               /* 本段走完，切段 */
    {
        s_phase++;
        s_beep_ms = 0U;
    }
}

#elif BUZZER_TONE_MODE == BUZZER_TONE_CLEAN

void buzzer_tick(void)
{
    if (s_phase >= (uint8_t)BUZZER_TONE_STEPS)
    {
        return;
    }

    if (s_left_ms == 0U)               /* 开新段 */
    {
        s_left_ms = ((s_phase & 1U) == 0U) ? BUZZER_CLEAN_BEEP_MS : BUZZER_CLEAN_GAP_MS;

        if ((s_phase & 1U) == 0U)      /* 发声段只在开头定一次频率，之后完全不动 */
        {
            buzzer_on(s_tone_freq[s_phase >> 1U], BUZZER_TONE_VOLUME);
        }
        else
        {
            buzzer_off();
        }
    }

    s_left_ms--;

    if (s_left_ms == 0U)               /* 本段走完，切段 */
    {
        s_phase++;
    }
}

#else
#error "BUZZER_TONE_MODE 只能取 BUZZER_TONE_CLEAN 或 BUZZER_TONE_ESC"
#endif
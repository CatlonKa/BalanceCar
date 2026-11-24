#include "LightShow.h"

#include <stdbool.h>

#include "LED.h"
#include "Buzzer.h"
#include "Attitude.h"
#include "event.h"
#include "main.h"

/* 150 BPM：八分音符 200 ms，16 步一段 = 3.2 s */
#define SHOW_BEAT_MS    200U
#define SHOW_STEPS      16U
#define SHOW_SCENE_MS   (SHOW_BEAT_MS * SHOW_STEPS)

/* 刷灯周期 25 ms（40 fps）；WS2812 一帧只要 61 us */
#define SHOW_FRAME_MS   25U

/* 场景切换/拍头白闪时长 */
#define SHOW_FLASH_MS   60U

/* 旋律音量 0..100 */
#define SHOW_VOLUME     60U

enum
{
    SHOW_RAINBOW = 0,
    SHOW_BREATH,
    SHOW_COMET,
    SHOW_STROBE,
    SHOW_HEART,
    SHOW_WIPE,
    SHOW_SCENE_COUNT
};

/* 十六步循环：Am - Em - Am - G，0 = 休止 */
static const uint16_t s_note[SHOW_STEPS] =
{
     440U,  659U,  880U, 1046U,
     494U,  659U,  784U,  659U,
     440U,  659U,  880U, 1318U,
     392U,  587U,  784U,    0U
};

static uint32_t s_frame_ms;              /* 上次刷灯时刻 */
static uint32_t s_show_ms;               /* 本次表演起点 */
static bool     s_playing;               /* 上一拍是否在表演 */
static uint8_t  s_sound_step = 0xFFU;    /* 已触发的音符序号，0xFF = 无 */

/* HSV -> RGB，色环 0..255，满饱和 */
static void hsv2rgb(uint8_t h, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t sect = (uint8_t)(h / 43U);
    uint8_t frac = (uint8_t)(h - sect * 43U);
    uint8_t q = (uint8_t)((uint16_t)v * frac / 43U);
    uint8_t t = (uint8_t)((uint16_t)v * (43U - frac) / 43U);

    switch (sect)
    {
    case 0U:  *r = v;  *g = q;  *b = 0U; break;
    case 1U:  *r = t;  *g = v;  *b = 0U; break;
    case 2U:  *r = 0U; *g = v;  *b = q;  break;
    case 3U:  *r = 0U; *g = t;  *b = v;  break;
    case 4U:  *r = q;  *g = 0U; *b = v;  break;
    default:  *r = v;  *g = 0U; *b = t;  break;
    }
}

/* 0..255 三角波 */
static uint8_t tri(uint32_t ms, uint16_t period)
{
    uint32_t half = period / 2U;
    uint32_t p = ms % period;

    return (uint8_t)(((p < half) ? p : (period - p)) * 255U / half);
}

/* 双脉冲心跳，周期 1000 ms */
static uint8_t heart(uint32_t p)
{
    if (p < 110U) { return (uint8_t)(p * 255U / 110U); }
    if (p < 190U) { return (uint8_t)((190U - p) * 255U / 80U); }
    if (p < 300U) { return (uint8_t)((p - 190U) * 255U / 110U); }
    if (p < 380U) { return (uint8_t)((380U - p) * 255U / 80U); }
    return 0U;
}

/* 一帧场景：只填两灯颜色，不发送 */
static void scene_render(uint8_t scene, uint8_t step, uint16_t ms_scene, uint32_t t)
{
    uint8_t r0 = 0U, g0 = 0U, b0 = 0U;
    uint8_t r1 = 0U, g1 = 0U, b1 = 0U;

    switch (scene)
    {
    case SHOW_RAINBOW:                  /* 色环旋转，拍头提亮 */
    {
        uint8_t v = ((ms_scene % SHOW_BEAT_MS) < SHOW_FLASH_MS) ? 255U : 190U;

        hsv2rgb((uint8_t)(t >> 3), v, &r0, &g0, &b0);
        hsv2rgb((uint8_t)((t >> 3) + 128U), v, &r1, &g1, &b1);
        break;
    }
    case SHOW_BREATH:                   /* 双灯反相呼吸 */
    {
        uint8_t k = tri(ms_scene, 1400U);

        hsv2rgb((uint8_t)(t >> 5), k, &r0, &g0, &b0);
        hsv2rgb((uint8_t)(t >> 5), (uint8_t)(255U - k), &r1, &g1, &b1);
        break;
    }
    case SHOW_COMET:                    /* 亮度在两灯间来回甩 */
    {
        uint8_t head = tri(ms_scene, 800U);

        hsv2rgb((uint8_t)(t >> 4), (uint8_t)(255U - head), &r0, &g0, &b0);
        hsv2rgb((uint8_t)(t >> 4), head, &r1, &g1, &b1);
        break;
    }
    case SHOW_STROBE:                   /* 拍点白闪 + 红蓝交替 */
    {
        uint16_t p = (uint16_t)(ms_scene % SHOW_BEAT_MS);

        if (p < 40U)
        {
            r0 = g0 = b0 = 255U;
            r1 = g1 = b1 = 255U;
        }
        else if (p < 110U)
        {
            if ((step & 1U) == 0U) { r0 = 255U; r1 = 255U; }
            else                   { b0 = 255U; b1 = 255U; }
        }
        break;
    }
    case SHOW_HEART:                    /* 红色心跳 */
    {
        uint8_t k = heart(ms_scene % 1000U);
        uint8_t gg = (uint8_t)(k >> 3);

        r0 = k; g0 = gg; b0 = 0U;
        r1 = k; g1 = gg; b1 = 0U;
        break;
    }
    default:                            /* SHOW_WIPE：逐格流水，两灯亮暗互换 */
    {
        uint8_t v0 = ((step & 1U) == 0U) ? 255U : 40U;

        hsv2rgb((uint8_t)(step * 16U), v0, &r0, &g0, &b0);
        hsv2rgb((uint8_t)(step * 16U), (uint8_t)(295U - v0), &r1, &g1, &b1);
        break;
    }
    }

    WS2812_SetColor(0U, r0, g0, b0);
    WS2812_SetColor(1U, r1, g1, b1);
}

/* 板载灯：LED1..3 跑马，LED_W1/W2 随拍闪 */
static void onboard(uint8_t step, uint16_t ms_scene)
{
    uint8_t pos = (uint8_t)((step >> 1U) % 3U);
    bool flash = (ms_scene % SHOW_BEAT_MS) < 80U;

    LED1((uint8_t)(pos == 0U));
    LED2((uint8_t)(pos == 1U));
    LED3((uint8_t)(pos == 2U));
    LED_W1(flash);
    LED_W2(flash);
}

static void all_off(void)
{
    DI_ALL_LED(0U, 0U, 0U);

    LED1(0U);
    LED2(0U);
    LED3(0U);
    LED_W1(0U);
    LED_W2(0U);
}

/* 未直立：红灯慢闪，只提示不发声 */
static void alarm_render(uint32_t now)
{
    bool on = (now % 600U) < 300U;

    if (on)
    {
        WS2812_SetColor(0U, 255U, 0U, 0U);
        WS2812_SetColor(1U, 255U, 0U, 0U);
    }
    else
    {
        WS2812_SetColor(0U, 0U, 0U, 0U);
        WS2812_SetColor(1U, 0U, 0U, 0U);
    }
    WS2812_Update(NUM_LED);

    LED1(on);
    LED2(on);
    LED3(on);
    LED_W1(on);
    LED_W2(on);

    if (!buzzer_is_busy())              /* 别掐掉起步自检音 */
    {
        buzzer_off();
    }
}

/* 每步只触发一次音符 */
static void show_sound(uint8_t step)
{
    if (step == s_sound_step)
    {
        return;
    }
    s_sound_step = step;

    if (buzzer_is_busy())               /* 自检音优先播完 */
    {
        return;
    }

    if (s_note[step] == 0U)
    {
        buzzer_off();
    }
    else
    {
        buzzer_on(s_note[step], SHOW_VOLUME);
    }
}

void LightShow_Init(void)
{
    s_frame_ms = 0U;
    s_show_ms = 0U;
    s_playing = false;
    s_sound_step = 0xFFU;
}

void LightShow_Update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t t;
    uint16_t ms_scene;
    uint8_t step;
    uint8_t scene;
    bool running = (car_run != 0U) && Attitude_IsUpright();

    /* 40 fps 限速，给 WS2812 DMA 留余量 */
    if ((int32_t)(now - s_frame_ms) < (int32_t)SHOW_FRAME_MS)
    {
        return;
    }
    s_frame_ms = now;

    if (running != s_playing)           /* 起停沿：剧本从头开始 */
    {
        s_playing = running;
        s_show_ms = now;
        s_sound_step = 0xFFU;

        if (!running && !buzzer_is_busy())
        {
            buzzer_off();
        }
    }

    if (!running)
    {
        if (car_run != 0U)
        {
            alarm_render(now);          /* 还没立起来 */
        }
        else
        {
            all_off();                  /* 停车：全部熄灭 */
        }
        return;
    }

    t = now - s_show_ms;
    ms_scene = (uint16_t)(t % SHOW_SCENE_MS);
    step = (uint8_t)(ms_scene / SHOW_BEAT_MS);
    scene = (uint8_t)((t / SHOW_SCENE_MS) % SHOW_SCENE_COUNT);

    if (ms_scene < SHOW_FLASH_MS)       /* 换场白闪 */
    {
        WS2812_SetColor(0U, 255U, 255U, 255U);
        WS2812_SetColor(1U, 255U, 255U, 255U);
    }
    else
    {
        scene_render(scene, step, ms_scene, t);
    }
    WS2812_Update(NUM_LED);

    onboard(step, ms_scene);
    show_sound(step);
}

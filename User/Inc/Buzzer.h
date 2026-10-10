#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "main.h"
#include "tim.h"

void buzzer_init(void);
void buzzer_on(uint16_t frequency, uint16_t volume);
void buzzer_off(void);

/* 启动自检音：由 buzzer_tick() 在 TIM5 中断里非阻塞推进。
   调用点：main() 上电一次、car_state() 里 KEY0 起步时 */
void buzzer_startup_tone(void);
void buzzer_tick(void);

/* ================== 自检音：选型 ================== */
#define BUZZER_TONE_CLEAN 0   /* 干净三声，纯方波，无调制 */
#define BUZZER_TONE_ESC   1   /* 电调风格，加颤音/抖动/占空比摆动 */
#define BUZZER_TONE_MODE  BUZZER_TONE_ESC

/* 三声基频，上行，两种模式共用（音高一样才比得出音色差别） */
#define BUZZER_TONE_FREQ1   750U
#define BUZZER_TONE_FREQ2  1350U
#define BUZZER_TONE_FREQ3  1800U
#define BUZZER_TONE_VOLUME   80U   /* 0..100；蜂鸣器直接挂引脚，再大只能改硬件 */

/* ---- 干净版：每声只在开头定一次频率，之后完全不动 ---- */
#define BUZZER_CLEAN_BEEP_MS 120U
#define BUZZER_CLEAN_GAP_MS   80U

/* ---- 电调版 ---- */
#define BUZZER_ESC_BEEP_MS   180U
#define BUZZER_ESC_GAP_MS     65U   /* 短一点，三声才像一串 */
#define BUZZER_ESC_ATK_MS     12U   /* 起振斜坡 */
#define BUZZER_ESC_REL_MS     30U   /* 收尾衰减，太长会模糊音高 */
#define BUZZER_ESC_VIB_MS     20U   /* 颤音周期 20ms → 50Hz（必须偶数） */
#define BUZZER_ESC_VIB_PCT     1U   /* 颤音深度 ±%；1% ≈ ±17 音分，大了就是走音 */
#define BUZZER_ESC_JIT_PCT     1U   /* 周期随机抖动 ±%，那点颗粒感 */
#define BUZZER_ESC_GROWL_MS   22U   /* 占空比摆动周期 22ms → 45Hz（必须偶数） */
/* 占空比摆幅同时就是音量摆幅：方波基波幅度 ∝ sin(π·d)，d=50% 最大。
   DUTY_MIN 越低 growl 越粗，但那段几乎是哑的 */
#define BUZZER_ESC_DUTY_MIN   25U
#define BUZZER_ESC_DUTY_MAX   50U

#endif
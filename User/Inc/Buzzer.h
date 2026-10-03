#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "main.h"
#include "tim.h"

void buzzer_init(void);
void buzzer_on(uint16_t frequency, uint16_t volume);
void buzzer_off(void);

/* 启动三声自检音；buzzer_tick() 每轮主循环调用推进序列，都不阻塞 */
void buzzer_startup_tone(void);
void buzzer_tick(void);

#endif
#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "main.h"
#include "tim.h"

void buzzer_init(void);
void buzzer_on(uint16_t frequency, uint16_t volume);
void buzzer_off(void);

#endif
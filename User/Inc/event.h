#ifndef __EVENT_H__
#define __EVENT_H__

#include "main.h"
#include "tim.h"
#include "encoder.h"
#include "key.h"
#include "Analog.h"
#include "Buzzer.h"

extern int16_t speedL;
extern int16_t speedR;
extern uint8_t car_run;

extern volatile uint8_t car_state_changed;

void event_init(void);
void car_state(void);

#endif 
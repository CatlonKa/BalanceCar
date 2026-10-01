#ifndef __EVENT_H__
#define __EVENT_H__

#include "main.h"
#include "tim.h"
#include "encoder.h"

extern int16_t speedL;
extern int16_t speedR;
extern uint8_t car_run;

void event_init(void);
void car_state(void);

#endif 
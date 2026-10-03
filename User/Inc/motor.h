#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "main.h"
#include "tim.h"

void motor_init(void);
void motor_drive(int16_t Left, int16_t Right);


#endif 
#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "main.h"
#include "tim.h"

void encoder_init(void);
int16_t encoder_get_delta_left(void);
int16_t encoder_get_delta_right(void);




#endif
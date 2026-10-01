#ifndef __IMU_H__
#define __IMU_H__

#include "main.h"

void imu_init(void);
void imu_transmit(uint8_t *data, uint16_t size);



#endif
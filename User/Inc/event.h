#ifndef __EVENT_H__
#define __EVENT_H__

#include "main.h"
#include "tim.h"
#include "gpio.h"
#include "encoder.h"
#include "key.h"
#include "Analog.h"
#include "Buzzer.h"
#include "imu.h"

extern int16_t speedL;
extern int16_t speedR;
extern uint8_t car_run;

extern volatile uint8_t car_state_changed;

// IMU 原始快照，主循环读（单位 LSB，顺序 X/Y/Z）
extern volatile int16_t imu_gyro[3];
extern volatile int16_t imu_acc[3];
extern volatile uint8_t imu_drdy_flag;

void event_init(void);
void car_state(void);
void imu_service(void);

#endif 
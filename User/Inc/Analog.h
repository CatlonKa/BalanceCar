#ifndef __ANALOG_H__
#define __ANALOG_H__

#include "main.h"
#include "adc.h"

/*
 * 原始比例推导值为 16.17：分压比 = 10k / (39k + 10k)
 * 而 Vpin 满量程是 3.3V 参考电压
 *
 * 由于电阻精度、VDDA 实际值等外部误差，实测会偏离，需要自己标定：
 *   1. 用万用表量出电池两端的真实电压 V_real；
 *   2. 读出当前代码算出来的电压 V_code；
 *   3. 新值 = 16.17 × V_real / V_code。
 * 分压是线性的，单点标定就够。
 */
#define ANALOG_FULL_SCALE_VOLTAGE 16.17f

float Analog_Read(void);


#endif
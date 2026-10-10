#ifndef __CONTROL_PID_H__
#define __CONTROL_PID_H__

#include "Control.h"

/* 直立环 PD：倾角误差 -> 推力 */
#define PID_PITCH_KP        600.0f
#define PID_PITCH_KD         12.0f

/* 速度环 PI：速度 -> 倾角给定，输出单位 rad */
#define PID_SPEED_KP          0.001f
#define PID_SPEED_KI          0.0005f
#define PID_SPEED_ILIM        0.05f     /* 积分项上限，rad */
#define PID_PITCH_SET_LIMIT   0.15f     /* 倾角给定上限，rad ≈ 8.6° */

/* 转向环 P：抑制偏转 */
#define PID_TURN_KP           8.0f

void Control_PID_Reset(void);
void Control_PID_Step(const Control_Input *in, Control_Output *out);

extern const Control_Algo Control_AlgoPID;

#endif

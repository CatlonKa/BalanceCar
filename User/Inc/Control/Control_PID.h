#ifndef __CONTROL_PID_H__
#define __CONTROL_PID_H__

#include "Control.h"

/* 直立环 PD：倾角误差 -> 推力 */
#define PID_PITCH_KP        600.0f
#define PID_PITCH_KD         12.0f

/* 速度环 PI：输出 = 倾角给定，单位 rad */
#define PID_SPEED_KP          0.3f
#define PID_SPEED_KI          0.02f
#define PID_SPEED_LIMIT     300.0f

/* 转向环 P：抑制偏转 */
#define PID_TURN_KP           8.0f

void Control_PID_Reset(void);
void Control_PID_Step(const Control_Input *in, Control_Output *out);

extern const Control_Algo Control_AlgoPID;

#endif

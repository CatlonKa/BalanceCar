#ifndef __CONTROL_LQR_H__
#define __CONTROL_LQR_H__

#include "Control.h"

/* u = -Kx，x = [pitch, pitch_rate, speed, turn_rate]；K 由离线解 Riccati 得到，
   整体缩放/转向行可微调，单个元素不要手拧 */
#define LQR_K_THRUST_PITCH    600.0f
#define LQR_K_THRUST_RATE      12.0f
#define LQR_K_THRUST_SPEED      0.3f
#define LQR_K_THRUST_TURN       0.0f

#define LQR_K_STEER_PITCH       0.0f
#define LQR_K_STEER_RATE        0.0f
#define LQR_K_STEER_SPEED       0.0f
#define LQR_K_STEER_TURN        8.0f

/* 只有 4 个状态、无积分项：速度存在静差，要消静差需扩成 5 状态 */

void Control_LQR_Reset(void);
void Control_LQR_Step(const Control_Input *in, Control_Output *out);

extern const Control_Algo Control_AlgoLQR;

#endif

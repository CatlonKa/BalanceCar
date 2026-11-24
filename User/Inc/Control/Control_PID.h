#ifndef __CONTROL_PID_H__
#define __CONTROL_PID_H__

#include "Control.h"

/* 直立环 PD：倾角误差 -> 推力。
   ωn = √(0.05KP−98)：KP=6000 → 14.2 rad/s (2.26 Hz)；ζ = 0.05KD/(2ωn) = 0.53
   角死区 = 起转 50 / KP = 0.48°（KP=3000 时 0.96°）
   ⚠️ 不要再降 KP：KP=3000 时整环闭环极点实部为正（实测表现为持续低频摆动）
      抖动幅度想再小 → 降下面的 SPEED_KP，不是降这里 */
#define PID_PITCH_KP        6000.0f
#define PID_PITCH_KD         300.0f

/* 速度环 PI：偏差 -> 倾角给定，输出单位 rad。
   ⚠️ KP 项直接产生 rad：不限幅时偏差 500 → 500×0.001 = 0.5 rad = 29°。
   抬 PITCH_KP 会减半内环 DC 增益（θp/δ = 0.05KP/(0.05KP−98)，6000 时 1.49，3000 时 2.88）。

   ❗本项是**抖动的主旋钮**（实测 + 闭环极点验证）：
     KPs  0.003  → 2.44 Hz  ζ=−0.06  发散
     KPs  0.002  → 2.19 Hz  ζ=+0.03  等幅振荡 ← 改前
     KPs  0.001  → 1.94 Hz  ζ=+0.25  收敛 ← 现在
     KPs 0.0005  → 1.99 Hz  ζ=+0.43  最稳，但响应慢一半
   权限与 KPs 无关：靠 ERR_LIM×KPs 维持，所以降 KPs 可以同时把 ERR_LIM 抬上去 */
#define PID_SPEED_KP        0.001f
#define PID_SPEED_KI        0.003f
#define PID_SPEED_ERR_LIM   160.0f    /* 计数/2ms；×KP = 0.16 rad（与改前同权限）。
                                         与 ILIM 合计 0.24 rad → 最大减速度 3.5 m/s²。
                                         线性段放宽到 160 计数（0.35 m/s），只在更大误差时削顶 */
#define PID_SPEED_ILIM      0.08f     /* 积分项上限，rad；4.6°。
                                         不宜再大：积分对着角死区会持续风积 */
#define PID_PITCH_SET_LIMIT 0.4f

/* 机械中值 = 真平衡角 θ₀：直接填「零推力站住时」的 pitch 读数（不要取反）。
   实测（串口 ch0，pwm≈0、speed=0）：-0.0906 rad = -5.19°。 */
#define PID_PITCH_TRIM_RAD     -0.0906f

/* 转向环 P：抑制偏转 */
#define PID_TURN_KP           0.0f//8.0f

/* 直行同步：轮速差 -> 差模。轮速差积分即两轮转角差，天然不漂（0 = 关闭） */
#define PID_SYNC_KP           0.0f

/* 偏航阻尼：陀螺 Z 轴 -> 差模，只压瞬时摆动，有零偏但不累积 */
#define PID_YAW_DAMP          0.0f

/* 运行期副本：初值取上面同名宏的缩写版，可在调试器里直接改写。
   PID_PITCH_KP -> pid_pitch_kp，PID_SPEED_ILIM -> pid_speed_ilim
   改宏 = 改上电初值（需重编译）；改变量 = 只本次运行有效（掉电复原） */
extern float pid_pitch_kp;
extern float pid_pitch_kd;
extern float pid_speed_kp;
extern float pid_speed_ki;
extern float pid_speed_err_lim;
extern float pid_speed_ilim;
extern float pid_pitch_set_limit;
extern float pid_pitch_trim_rad;
extern float pid_turn_kp;
extern float pid_sync_kp;
extern float pid_yaw_damp;

void Control_PID_Reset(void);
void Control_PID_Step(const Control_Input *in, Control_Output *out);

/* 上拍倾角给定，rad；仅供遥测 */
float Control_PID_GetPitchSet(void);

extern const Control_Algo Control_AlgoPID;

#endif

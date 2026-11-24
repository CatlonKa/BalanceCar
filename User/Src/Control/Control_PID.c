#include "Control_PID.h"

/* 参数运行期副本：初值即同名宏（.h 里调参），可被调试器/上位机直接改写 */
float pid_pitch_kp        = PID_PITCH_KP;
float pid_pitch_kd        = PID_PITCH_KD;
float pid_speed_kp        = PID_SPEED_KP;
float pid_speed_ki        = PID_SPEED_KI;
float pid_speed_err_lim   = PID_SPEED_ERR_LIM;
float pid_speed_ilim      = PID_SPEED_ILIM;
float pid_pitch_set_limit = PID_PITCH_SET_LIMIT;
float pid_pitch_trim_rad  = PID_PITCH_TRIM_RAD;
float pid_turn_kp         = PID_TURN_KP;
float pid_sync_kp         = PID_SYNC_KP;
float pid_yaw_damp        = PID_YAW_DAMP;

static float s_speed_int;  // 速度环的积分项
static float s_pitch_set;  // 上拍倾角给定，仅供遥测

static float pid_clamp(float v, float lim)
{
    if (v > lim)
    {
        return lim;
    }
    if (v < -lim)
    {
        return -lim;
    }
    return v;
}

void Control_PID_Reset(void)
{
    s_speed_int = 0.0f;
    s_pitch_set = 0.0f;
}

void Control_PID_Step(const Control_Input *in, Control_Output *out)
{
    float speed_err;   //   实际 - 目标
    float pitch_set;   //	目标倾角
    float thrust;      //   直立控制量
    float steer;       //   转向控制量

    /* 速度环 PI：偏差 -> 倾角给定；叠加机械中值。
       偏差必须是「实际 - 目标」：pitch_set 是目标倾角，稳态时 pitch 跟到 pitch_set，
       pitch_set 越正 -> 前倾越多 -> 向前加速（a = g·tan(pitch_set - 机械中值)）。
       写成「目标 - 实际」是正反馈：越慢 -> pitch_set 越负 -> 向后加速 -> 更慢。 */
    speed_err = pid_clamp(in->speed - in->target_speed, pid_speed_err_lim);
    s_speed_int += speed_err * pid_speed_ki * in->dt;
    s_speed_int = pid_clamp(s_speed_int, pid_speed_ilim);
    pitch_set = pid_pitch_trim_rad - (speed_err * pid_speed_kp + s_speed_int);
    pitch_set = pid_clamp(pitch_set, pid_pitch_set_limit);

    /* 直立环 PD：倾角 -> 推力 */
    thrust = (in->pitch - pitch_set) * pid_pitch_kp + in->pitch_rate * pid_pitch_kd;

    s_pitch_set = pitch_set;

      /* 转向：同步项保直行（积分即角度差，不漂），阻尼项压瞬时摆动 */
    steer = -pid_sync_kp * (in->speed_left - in->speed_right)
            - pid_yaw_damp * in->yaw_rate;

    out->left = thrust + steer;
    out->right = thrust - steer;
}

float Control_PID_GetPitchSet(void)
{
    return s_pitch_set;
}

const Control_Algo Control_AlgoPID =
{
    "PID",
    Control_PID_Reset,
    Control_PID_Step
};

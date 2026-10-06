#include "Control_PID.h"

static float s_speed_int;

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
}

void Control_PID_Step(const Control_Input *in, Control_Output *out)
{
    float pitch_set;
    float thrust;
    float steer;

    /* 速度环 PI：速度 -> 倾角给定 */
    s_speed_int += in->speed * PID_SPEED_KI * in->dt;
    s_speed_int = pid_clamp(s_speed_int, PID_SPEED_LIMIT);
    pitch_set = -(in->speed * PID_SPEED_KP + s_speed_int);
    pitch_set = pid_clamp(pitch_set, PID_SPEED_LIMIT);

    /* 直立环 PD：倾角 -> 推力 */
    thrust = (in->pitch - pitch_set) * PID_PITCH_KP + in->pitch_rate * PID_PITCH_KD;

    /* 转向环 P：抑制偏转 */
    steer = -in->turn_rate * PID_TURN_KP;

    out->left = thrust + steer;
    out->right = thrust - steer;
}

const Control_Algo Control_AlgoPID =
{
    "PID",
    Control_PID_Reset,
    Control_PID_Step
};

#include "Control.h"
#include "event.h"
#include "motor.h"

#if CONTROL_HAVE_ATTITUDE
#include "Attitude.h"
#endif

extern const Control_Algo Control_AlgoPID;
extern const Control_Algo Control_AlgoLQR;

static const Control_Algo *const s_algos[CONTROL_ALGO_COUNT] =
{
    &Control_AlgoPID,
    &Control_AlgoLQR
};

static uint8_t s_algo = CONTROL_ALGO_PID;

static int16_t control_limit(float v)
{
    int16_t r;

    if (v > (float)CONTROL_PWM_MAX)
    {
        v = (float)CONTROL_PWM_MAX;
    }
    else if (v < -(float)CONTROL_PWM_MAX)
    {
        v = -(float)CONTROL_PWM_MAX;
    }
    r = (int16_t)v;

    if (r > 0 && r < CONTROL_DEADZONE)
    {
        r = CONTROL_DEADZONE;
    }
    else if (r < 0 && r > -CONTROL_DEADZONE)
    {
        r = -CONTROL_DEADZONE;
    }
    return r;
}

/* 采集观测量：只读已发布快照，不碰硬件 */
static void control_collect(Control_Input *in)
{
#if CONTROL_HAVE_ATTITUDE
    in->pitch = Attitude_GetPitch();
    in->pitch_rate = Attitude_GetPitchRate();
#else
    in->pitch = 0.0f;
    in->pitch_rate = 0.0f;
#endif
    in->speed = 0.5f * (float)(speedL + speedR);
    in->turn_rate = 0.5f * (float)(speedL - speedR);
    in->dt = CONTROL_DT_S;
}

void Control_Init(void)
{
    Control_Reset();
}

void Control_Reset(void)
{
    s_algos[s_algo]->reset();
}

void Control_SetAlgo(uint8_t idx)
{
    if (idx >= CONTROL_ALGO_COUNT)
    {
        return;
    }
    s_algo = idx;
    s_algos[s_algo]->reset();
}

uint8_t Control_GetAlgo(void)
{
    return s_algo;
}

void Control_Update(void)
{
    Control_Input in;
    Control_Output out;

    if (!car_run)                 /* 车轮归 UI：不参与控制 */
    {
        motor_drive(0, 0);
        return;
    }

    control_collect(&in);

    out.left = 0.0f;
    out.right = 0.0f;
    s_algos[s_algo]->step(&in, &out);

    motor_drive(control_limit(out.left), control_limit(out.right));
}

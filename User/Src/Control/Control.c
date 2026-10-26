#include "Control.h"
#include "event.h"
#include "motor.h"
#include "Attitude.h"

extern const Control_Algo Control_AlgoPID;
extern const Control_Algo Control_AlgoLQR;

static const Control_Algo *const s_algos[CONTROL_ALGO_COUNT] =
{
    &Control_AlgoPID,
    &Control_AlgoLQR
};

static uint8_t s_algo = CONTROL_ALGO_DEFAULT;
static uint8_t s_mode = CONTROL_MODE_DEFAULT;

static int16_t control_limit(float v)
{
    if (v > (float)CONTROL_PWM_MAX)
    {
        v = (float)CONTROL_PWM_MAX;
    }
    else if (v < -(float)CONTROL_PWM_MAX)
    {
        v = -(float)CONTROL_PWM_MAX;
    }
    return (int16_t)v;
}

/* 采集观测量：只读已发布快照，不碰硬件 */
static void control_collect(Control_Input *in)
{
    in->pitch = Attitude_GetPitch();
    in->pitch_rate = Attitude_GetPitchRate();
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

void Control_SetMode(uint8_t mode)
{
    if (mode > CONTROL_MODE_MANUAL)
    {
        return;
    }
    s_mode = mode;
    s_algos[s_algo]->reset();
}

uint8_t Control_GetMode(void)
{
    return s_mode;
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

    if (s_mode != CONTROL_MODE_AUTO)   /* 手动：遥控输入还没接，先不驱动 */
    {
        s_algos[s_algo]->reset();
        motor_drive(0, 0);
        return;
    }

    /* 安全联锁：姿态没标定完或车身不在直立窗口就一律不驱动。
       标定期间 GetPitch() 恒 0，等于「完美直立」，接上就会恒定推力飞车。 */
    if (Attitude_IsCalibrating() || !Attitude_IsUpright())
    {
        s_algos[s_algo]->reset();    /* 顺便清积分，防退出联锁时突跳 */
        motor_drive(0, 0);
        return;
    }

    control_collect(&in);

    out.left = 0.0f;
    out.right = 0.0f;
    s_algos[s_algo]->step(&in, &out);

    motor_drive(control_limit(out.left), control_limit(out.right));
}

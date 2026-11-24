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

static int16_t s_pwm_common;   // 上拍共模输出，仅供遥测

static float s_target_speed;   // 上层目标量，跨周期保持
static float s_target_yaw_rate;
static float s_target_steer;

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

/* 输出级：先补共模死区（加偏移而非 clamp 抬底，保住响应线性），
   再加差模，最后逐轮限幅。顺序不能反：死区补到差模上会把小 steer 放大成 ±d0。 */
static void control_output(float thrust, float steer, int16_t *left, int16_t *right)
{
    if (thrust > 0.0f)
    {
        thrust += (float)CONTROL_DEADZONE;
    }
    else if (thrust < 0.0f)
    {
        thrust -= (float)CONTROL_DEADZONE;
    }

    *left = control_limit(thrust + steer);
    *right = control_limit(thrust - steer);
}

/* 不驱动：清共模快照并让两轮停住。不在此清积分 —— 各分支需求不同 */
static void control_stop(void)
{
    s_pwm_common = 0;
    motor_drive(0, 0);
}

/* 采集观测量与目标量：只读已发布快照，不碰硬件 */
static void control_collect(Control_Input *in)
{
    int16_t sl = speedL;   // 一次读入，免得均值/差值取自不同拍
    int16_t sr = speedR;

    in->pitch = Attitude_GetPitch();
    in->pitch_rate = Attitude_GetPitchRate();
    in->yaw_rate = Attitude_GetYawRate();
    in->speed_left = (float)sl;
    in->speed_right = (float)sr;
    in->speed = 0.5f * (float)(sl + sr);
    in->turn_rate = 0.5f * (float)(sl - sr);
    in->target_speed = s_target_speed;
    in->target_yaw_rate = s_target_yaw_rate;
    in->target_steer = s_target_steer;
    in->dt = CONTROL_DT_S;

#if (CONTROL_TEST_ENABLE)
    in->target_speed = CONTROL_TEST_SPEED;
    in->target_steer = CONTROL_TEST_STEER;
#endif
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

void Control_SetTarget(float speed, float yaw_rate, float steer)
{
    s_target_speed = speed;
    s_target_yaw_rate = yaw_rate;
    s_target_steer = steer;
}

int16_t Control_GetPwmCommon(void)
{
    return s_pwm_common;
}

void Control_Update(void)
{
    Control_Input in;
    Control_Output out;
    int16_t pl;
    int16_t pr;

    if (!car_run)                 /* 车轮归 UI：不参与控制 */
    {
        /* 必须清积分：停车期间 Control 不跑，积分会保留上次顶满的值，
           下次启动 pitch_set 直接偏 trim±0.08 rad（4.6°）→ 起步突跳冲出去 */
        s_algos[s_algo]->reset();
        control_stop();
        return;
    }

    if (s_mode != CONTROL_MODE_AUTO)   /* 手动：遥控输入还没接，先不驱动 */
    {
        s_algos[s_algo]->reset();
        control_stop();
        return;
    }

    /* 安全联锁：标定没完成或车已倒下（倾角 > 60°）就一律不驱动。
       标定期间 GetPitch() 恒 0，等于「完美直立」，接上就会恒定推力飞车。 */
    if (Attitude_IsCalibrating() || !Attitude_IsUpright())
    {
        s_algos[s_algo]->reset();    /* 顺便清积分，防退出联锁时突跳 */
        control_stop();
        return;
    }

    control_collect(&in);

    out.left = 0.0f;
    out.right = 0.0f;
    s_algos[s_algo]->step(&in, &out);

    /* 拆共模/差模：死区只补推路，补到差模会把小 steer 放大成 ±d0 甩车 */
    control_output(0.5f * (out.left + out.right),
                   0.5f * (out.left - out.right), &pl, &pr);

    s_pwm_common = (int16_t)(((int32_t)pl + (int32_t)pr) / 2);   /* 共模，仅供遥测 */
    motor_drive(pl, pr);
}

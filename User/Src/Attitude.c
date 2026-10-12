#include "Attitude.h"
#include "event.h"

#include <math.h>

/* 陀螺 ±500 dps → rad/s 每 LSB：17.50 mdps × π/180 */
#define ATTITUDE_GYRO_LSB_TO_RADS    3.054326e-4f

/* 加速度 ±4 g → mg 每 LSB，用来把静止判据换成 LSB */
#define ATTITUDE_ACC_MG_PER_LSB      0.122f

static float s_pitch;                   /* rad，互补滤波输出 */
static float s_pitch_rate;              /* rad/s */
static float s_gyro_bias[3];            /* LSB，标定得到的零偏 */
static float s_calib_sum[3];            /* LSB，标定累加 */
static uint16_t s_calib_count;
static bool s_calibrating;

static void attitude_calib_restart(void)
{
    s_calib_sum[0] = 0.0f;
    s_calib_sum[1] = 0.0f;
    s_calib_sum[2] = 0.0f;
    s_calib_count = 0U;
}

void Attitude_Init(void)
{
    s_pitch = 0.0f;
    s_pitch_rate = 0.0f;
    s_gyro_bias[0] = 0.0f;
    s_gyro_bias[1] = 0.0f;
    s_gyro_bias[2] = 0.0f;
    attitude_calib_restart();
    s_calibrating = true;
}

void Attitude_Calibrate(void)
{
    attitude_calib_restart();
    s_calibrating = true;
}

bool Attitude_IsCalibrating(void)
{
    return s_calibrating;
}

float Attitude_GetPitch(void)
{
    return s_pitch;
}

float Attitude_GetPitchRate(void)
{
    return s_pitch_rate;
}

void Attitude_Update(void)
{
    float gyro[3];
    float acc[3];
    float gyro_sq;
    float acc_mg;
    float rate;
    float tilt;

    /* 快照：TIM5(prio 3) 高于 EXTI9_5(prio 5)，读这 6 个字期间不会被 IMU 写入打断 */
    gyro[0] = (float)imu_gyro[0];
    gyro[1] = (float)imu_gyro[1];
    gyro[2] = (float)imu_gyro[2];
    acc[0] = (float)imu_acc[0];
    acc[1] = (float)imu_acc[1];
    acc[2] = (float)imu_acc[2];

    /* 绕 PITCH 轴的倾角：两轴 LSB 比值，量纲自动抵消，不需要 g */
    tilt = atan2f(acc[ATTITUDE_TILT_NUM_AXIS], acc[ATTITUDE_TILT_DEN_AXIS]) * ATTITUDE_PITCH_SIGN;

    if (s_calibrating)
    {
        gyro_sq = gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2];
        acc_mg = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]) * ATTITUDE_ACC_MG_PER_LSB;

        /* 在动就整段丢弃重来：宁可标不出来，也不能把运动当成零偏 */
        if (gyro_sq > (float)(ATTITUDE_CALIB_GYRO_MAX_LSB * ATTITUDE_CALIB_GYRO_MAX_LSB) ||
            acc_mg < (float)ATTITUDE_CALIB_ACC_MIN_MG ||
            acc_mg > (float)ATTITUDE_CALIB_ACC_MAX_MG)
        {
            attitude_calib_restart();
            s_pitch_rate = 0.0f;
            return;
        }

        s_calib_sum[0] += gyro[0];
        s_calib_sum[1] += gyro[1];
        s_calib_sum[2] += gyro[2];
        ++s_calib_count;

        if (s_calib_count >= ATTITUDE_CALIB_SAMPLES)
        {
            float inv = 1.0f / (float)s_calib_count;

            s_gyro_bias[0] = s_calib_sum[0] * inv;
            s_gyro_bias[1] = s_calib_sum[1] * inv;
            s_gyro_bias[2] = s_calib_sum[2] * inv;
            s_calibrating = false;
            s_pitch = tilt;         /* 用当前倾角起步，避免标定结束瞬间跳变 */
        }

        s_pitch_rate = 0.0f;
        return;
    }

    rate = (gyro[ATTITUDE_PITCH_AXIS] - s_gyro_bias[ATTITUDE_PITCH_AXIS]) *
           ATTITUDE_GYRO_LSB_TO_RADS * ATTITUDE_RATE_SIGN;

    s_pitch_rate = rate;
    s_pitch = ATTITUDE_ALPHA * (s_pitch + rate * ATTITUDE_DT_S) +
              (1.0f - ATTITUDE_ALPHA) * tilt;
}

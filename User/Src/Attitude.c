#include "Attitude.h"
#include "event.h"
#include "imu.h"
#include <math.h>


/* 陀螺 ±500 dps → rad/s 每 LSB：17.50 mdps × π/180 */
#define ATTITUDE_GYRO_LSB_TO_RADS    3.054326e-4f

/* 加速度 ±4 g → mg 每 LSB，用来把静止判据换成 LSB */
#define ATTITUDE_ACC_MG_PER_LSB      0.122f

/* 阻塞标定重试上限，防 IMU 没通时死等（5000 × 1ms = 5s） */
#define ATTITUDE_CALIB_RETRY_MAX     5000U

/* 防倒窗口：|pitch| 上限（rad）与重力落在 Y-Z 平面的最小占比。
   侧躺时 acc[1]、acc[2] 都趋 0，单轴 pitch 会误报「直立」，
   必须靠 Y-Z 占比识别：直立≈1.0，侧躺 90°≈0，侧倾 60°≈0.5。 */
#define ATTITUDE_UPRIGHT_MAX_RAD     1.047f   /* 60° */
#define ATTITUDE_UPRIGHT_MIN_YZ      0.5f     /* 约 60° 以内侧倾 */
#define ATTITUDE_PI                  3.14159265f

static float s_pitch;             // rad，融合后的俯仰角，前倾为正
static float s_pitch_rate;        // rad/s，陀螺扣零偏后的俯仰角速度
static float s_gyro_bias[3];      // LSB，标定得到的陀螺零偏，XYZ
static bool s_gyro_calibrating;   // true = 标定未完成或失败，此时姿态输出恒 0
static float s_yz_ratio;          // 重力在 Y-Z 平面的占比，0=完全侧躺，1=无侧倾

/* 读一帧 12B：陀螺 XYZ + 加速度 XYZ，小端。
   标定期直接读 SPI：主循环刷屏时快照会重复，500 个样本里只有几十个不同值。 */
static bool attitude_imu_read(float *gyro, float *acc)
{
    uint8_t buf[12];
    uint8_t i;

    if (!imu_read_data(buf, 12U))
    {
        return false;
    }
    for (i = 0U; i < 3U; i++)
    {
        gyro[i] = (float)(int16_t)((uint16_t)buf[2U * i + 1U] << 8 | buf[2U * i]);
        acc[i]  = (float)(int16_t)((uint16_t)buf[6U + 2U * i + 1U] << 8 | buf[6U + 2U * i]);
    }
    return true;
}

/* 阻塞标定：静止判据只看模长与角速度，与姿态无关，躺着也能标。成功返回 true。 */
static bool attitude_gyro_calibrate(void)
{
    float gyro[3];
    float acc[3] = {0.0f, 0.0f, 1.0f};     // 读不到时的兜底：按已直立处理
    float sum[3] = {0.0f, 0.0f, 0.0f};
    float acc_mg;
    float gyro_sq;
    uint16_t count = 0U;
    uint16_t retry = 0U;
    uint8_t i;

    while (count < ATTITUDE_GYRO_CALIB_SAMPLES)
    {
        if (++retry > ATTITUDE_CALIB_RETRY_MAX)
        {
            break;
        }
        HAL_Delay(1U);
        if (!attitude_imu_read(gyro, acc))
        {
            continue;
        }

        acc_mg = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]) *
                 ATTITUDE_ACC_MG_PER_LSB;
        gyro_sq = gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2];

        /* 在动就整段丢弃重来：宁可标不出来，也不能把运动当成零偏 */
        if (gyro_sq > (float)(ATTITUDE_GYRO_CALIB_MAX_LSB * ATTITUDE_GYRO_CALIB_MAX_LSB) ||
            acc_mg < (float)ATTITUDE_ACC_CALIB_MIN_MG ||
            acc_mg > (float)ATTITUDE_ACC_CALIB_MAX_MG)
        {
            count = 0U;
            sum[0] = 0.0f;
            sum[1] = 0.0f;
            sum[2] = 0.0f;
            continue;
        }

        sum[0] += gyro[0];
        sum[1] += gyro[1];
        sum[2] += gyro[2];
        ++count;
    }

    if (count == 0U)
    {
        /* IMU 没通：零偏保持 0，标定标志留给上层当故障信号 */
        s_gyro_bias[0] = 0.0f;
        s_gyro_bias[1] = 0.0f;
        s_gyro_bias[2] = 0.0f;
        return false;
    }

    for (i = 0U; i < 3U; i++)
    {
        s_gyro_bias[i] = sum[i] / (float)count;
    }

    /* 起算角只取最后一张静止样本，所以 atan2f 放到循环外算一次就够了 */
    s_pitch = atan2f(acc[ATTITUDE_ACC_TILT_NUM_AXIS], acc[ATTITUDE_ACC_TILT_DEN_AXIS]) *
              ATTITUDE_ACC_PITCH_SIGN;
    return true;
}

void Attitude_Init(void)
{
    s_pitch = 0.0f;
    s_pitch_rate = 0.0f;
    s_yz_ratio = 0.0f;
    s_gyro_bias[0] = 0.0f;
    s_gyro_bias[1] = 0.0f;
    s_gyro_bias[2] = 0.0f;

    /* 阻塞约 0.5 s。失败时标志保持 true，上层据此禁止驱动。 */
    s_gyro_calibrating = true;
    if (attitude_gyro_calibrate())
    {
        s_gyro_calibrating = false;
    }
}

void Attitude_Calibrate(void)
{
    /* 与 Init 完全同一套动作：重标零偏、清零、按当前重力重置起算角 */
    Attitude_Init();
}

bool Attitude_IsCalibrating(void)
{
    return s_gyro_calibrating;
}

bool Attitude_IsUpright(void)
{
    /* 两个条件都要：pitch 在窗口内，且重力确实落在 Y-Z 平面（没大幅侧倾）。
       侧躺时 pitch 可能是 0，只靠它会误判直立。 */
    return !s_gyro_calibrating &&
           fabsf(s_pitch) <= ATTITUDE_UPRIGHT_MAX_RAD &&
           s_yz_ratio >= ATTITUDE_UPRIGHT_MIN_YZ;
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
    float gyro[3];      //陀螺仪原始数据快照，LSB，未扣零偏
    float acc[3];       //加速度计原始数据快照，LSB
    float gyro_rate;    //陀螺算出的角速度，rad/s
    float acc_tilt;     //加速度算出的倾角，rad，等价于 pitch 的测量值

    if (s_gyro_calibrating)
    {
        /* 标定未完成或失败（IMU 没通）：输出恒 0，由上层联锁禁止驱动。
           不要在这里继续积分，否则「零快照」会被当成完美直立。
           提前返回也省掉本拍的 atan2f。 */
        s_pitch = 0.0f;
        s_pitch_rate = 0.0f;
        return;
    }

    gyro[0] = (float)imu_gyro[0];
    gyro[1] = (float)imu_gyro[1];
    gyro[2] = (float)imu_gyro[2];
    acc[0] = (float)imu_acc[0];
    acc[1] = (float)imu_acc[1];
    acc[2] = (float)imu_acc[2];

    /* 绕 PITCH 轴的倾角：两轴 LSB 比值，量纲自动抵消，不需要 g */
    acc_tilt = atan2f(acc[ATTITUDE_ACC_TILT_NUM_AXIS], acc[ATTITUDE_ACC_TILT_DEN_AXIS]) *
               ATTITUDE_ACC_PITCH_SIGN;

    /* 重力在倾角平面内的占比：侧躺时分子分母都趋 0，这个值也趋 0。
       没有它就无法区分「直立」与「侧躺」，因为两者 pitch 都是 0。
       用平方比开一次方：比「两个开方再相除」少一次 sqrtf。 */
    {
        const float in_plane_sq = acc[ATTITUDE_ACC_TILT_NUM_AXIS] * acc[ATTITUDE_ACC_TILT_NUM_AXIS] +
                                  acc[ATTITUDE_ACC_TILT_DEN_AXIS] * acc[ATTITUDE_ACC_TILT_DEN_AXIS];
        const float all_sq = acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2];

        s_yz_ratio = (all_sq > 1.0f) ? sqrtf(in_plane_sq / all_sq) : 0.0f;
    }

    gyro_rate = (gyro[ATTITUDE_GYRO_PITCH_AXIS] - s_gyro_bias[ATTITUDE_GYRO_PITCH_AXIS]) *
                ATTITUDE_GYRO_LSB_TO_RADS * ATTITUDE_GYRO_RATE_SIGN;

    s_pitch_rate = gyro_rate;

    /* 角度差先归一化到 ±π 再融合：在 ±180° 附近 acc_tilt 会跨符号
       （+180 与 -179 物理上只差 1°），直接相减会每拍扫掉好几度。 */
    {
        float diff = acc_tilt - s_pitch;

        if (diff > ATTITUDE_PI)
        {
            diff -= 2.0f * ATTITUDE_PI;
        }
        else if (diff < -ATTITUDE_PI)
        {
            diff += 2.0f * ATTITUDE_PI;
        }
        s_pitch += ATTITUDE_ALPHA * gyro_rate * ATTITUDE_DT_S +
                   (1.0f - ATTITUDE_ALPHA) * diff;
    }
}

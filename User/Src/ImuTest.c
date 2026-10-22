#include "ImuTest.h"
#include "event.h"
#include "imu.h"
#include "uart.h"

#include <math.h>
#include <string.h>

/* 陀螺 ±500 dps → rad/s 每 LSB：17.50 mdps × π/180
   刻意与 Attitude.h 各写一份：本模块整体可删，不留耦合 */
#define IMU_TEST_GYRO_LSB_TO_RADS   3.054326e-4f

/* 加速度 ±4 g → mg 每 LSB，只用于标定期静止判据 */
#define IMU_TEST_ACC_MG_PER_LSB     0.122f

/* 陀螺静止判据：三轴合成超此值视为在动（60 LSB ≈ 1 dps） */
#define IMU_TEST_GYRO_QUIET_LSB     60

/*
 * 方向宏（已按实测定好，见 ImuTest.h 的对照表）：
 *   PITCH 前倾为正；ROLL 右倾为正；YAW 右转为正
 */
#define IMU_TEST_SIGN_ROLL          (-1.0f)
#define IMU_TEST_SIGN_PITCH         (+1.0f)
#define IMU_TEST_SIGN_YAW           (-1.0f)

/* 静止标定 300 × 1 ms ≈ 0.3 s */
#define IMU_TEST_CALIB_SAMPLES      300U
#define IMU_TEST_CALIB_DELAY_MS     1U

/* 标定重试上限，防止 IMU 没通时死循环（3000 × 1 ms ≈ 3 s） */
#define IMU_TEST_CALIB_RETRY_MAX    3000U

/* 发送频率：3 个 float(16 B) @100 Hz ≈ 1.6 kB/s，115200 很宽裕 */
#define IMU_TEST_SEND_HZ            100U

/* 3D 模型通道数：roll / pitch / yaw */
#define IMU_TEST_CH_COUNT           3U

/* yaw 积分间隔上限，防卡顿或首帧跳变 */
#define IMU_TEST_DT_MAX_MS          50U

static float s_gyro_bias[3];        /* LSB，三轴零偏 */
static float s_yaw;                 /* rad，陀螺 Z 积分，会漂 */
static uint32_t s_last_tick;        /* ms，上次发送时刻 */

/* JustFloat：n 个小端 float + 帧尾 00 00 80 7F；小端机上 memcpy 即是目标格式 */
static void imu_test_send(const float *v, uint8_t n)
{
    static const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};
    uint8_t buf[4U * 8U + 4U];

    memcpy(buf, v, 4U * n);
    memcpy(&buf[4U * n], tail, 4U);
    uart_send(buf, (uint16_t)(4U * n + 4U));
}

/* 读一帧 12 B：陀螺 XYZ 6 B + 加速度 XYZ 6 B，小端 */
static bool imu_test_read(float *gyro, float *acc)
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

void ImuTest_Init(void)
{
    float gyro[3];
    float acc[3];
    float sum[3] = {0.0f, 0.0f, 0.0f};
    float acc_mg;
    float gyro_sq;
    uint16_t count = 0U;
    uint16_t retry = 0U;
    uint8_t i;

    s_yaw = 0.0f;
    s_last_tick = HAL_GetTick();

    /* 静止判据只看模长与角速度，**与姿态无关**：躺着、侧躺、倒扣都能标 */
    while (count < IMU_TEST_CALIB_SAMPLES)
    {
        if (++retry > IMU_TEST_CALIB_RETRY_MAX)
        {
            break;
        }
        HAL_Delay(IMU_TEST_CALIB_DELAY_MS);
        if (!imu_test_read(gyro, acc))
        {
            continue;
        }

        acc_mg = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]) * IMU_TEST_ACC_MG_PER_LSB;
        gyro_sq = gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2];
        if (acc_mg < 800.0f || acc_mg > 1200.0f ||
            gyro_sq > (float)(IMU_TEST_GYRO_QUIET_LSB * IMU_TEST_GYRO_QUIET_LSB))
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
        /* 没标出来就按 0 处理，至少还能看角度 */
        s_gyro_bias[0] = 0.0f;
        s_gyro_bias[1] = 0.0f;
        s_gyro_bias[2] = 0.0f;
        return;
    }

    for (i = 0U; i < 3U; i++)
    {
        s_gyro_bias[i] = sum[i] / (float)count;
    }
}

void ImuTest_Update(void)
{
    float acc[3];
    float f[IMU_TEST_CH_COUNT];
    float yaw_rate;
    uint32_t now;
    uint32_t dt_ms;

    /* 限频：整块（积分 + 发送）都放在这里，不做两套节拍 */
    now = HAL_GetTick();
    if ((uint32_t)(now - s_last_tick) < (1000U / IMU_TEST_SEND_HZ))
    {
        return;
    }
    dt_ms = (uint32_t)(now - s_last_tick);
    if (dt_ms > IMU_TEST_DT_MAX_MS)
    {
        dt_ms = IMU_TEST_DT_MAX_MS;
    }
    s_last_tick = now;

    acc[0] = (float)imu_acc[0];
    acc[1] = (float)imu_acc[1];
    acc[2] = (float)imu_acc[2];

    /*
     * IMU 实际安装：X = 车左侧、Y = 车尾、Z = 上（右手系，实测确认）。
     *   → 车头上下俯仰 = 绕 X 轴 = atan2(acc[1], acc[2])
     *   → 左右侧倾     = 绕 Y 轴 = atan2(-acc[0], sqrt(acc[1]² + acc[2]²))
     * 注意：Attitude.c 里那个「pitch」也是这个绕 X 轴的角，两者数值相同。
     */
    f[IMU_TEST_CH_ROLL]  = IMU_TEST_SIGN_ROLL *
                           atan2f(-acc[0], sqrtf(acc[1] * acc[1] + acc[2] * acc[2]));
    f[IMU_TEST_CH_PITCH] = IMU_TEST_SIGN_PITCH * atan2f(acc[1], acc[2]);

    /* yaw 无绝对参考，纯积分，只用来确认 Z 轴方向 */
    yaw_rate = ((float)imu_gyro[2] - s_gyro_bias[2]) * IMU_TEST_GYRO_LSB_TO_RADS;
    s_yaw += IMU_TEST_SIGN_YAW * yaw_rate * ((float)dt_ms * 0.001f);
    f[IMU_TEST_CH_YAW] = s_yaw;

    imu_test_send(f, IMU_TEST_CH_COUNT);
}

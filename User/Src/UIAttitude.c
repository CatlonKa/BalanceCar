#include "UIAttitude.h"
#include "event.h"
#include "imu.h"
#include "main.h"

#include <math.h>

/* 四元数：体 → 世界。w 为标量，顺序 (w,x,y,z)。 */
static float s_qw = 1.0f;
static float s_qx;
static float s_qy;
static float s_qz;

static float s_bias[3];         /* LSB，三轴陀螺零偏 */
static bool s_ready;            /* 标定完成后为 true */
static uint32_t s_last_cycle;   /* DWT->CYCCNT 快照 */
static uint32_t s_seen_seq;     /* 已消费的 imu_seq，用于判断新样本 */

/* 标定累加器：跨 UIAttitude_CalibStep() 多次调用保持 */
static uint16_t s_calib_count;  /* 已累计的连续静止样本数 */
static uint16_t s_calib_retry;  /* 总尝试次数，用于超时保护 */
static float s_calib_sum[3];    /* 陀螺累加 */
static float s_calib_acc[3];    /* 加速度累加 */

/* 归一化三轴向量；返回是否有效（非零） */
static bool ui_attitude_normalize(float *v)
{
    float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (n < 1.0e-6f)
    {
        return false;
    }
    v[0] /= n;
    v[1] /= n;
    v[2] /= n;
    return true;
}

/* 按当前四元数算机体系下的重力方向估计：R^T · (0,0,1) = R 的第三行 */
static void ui_attitude_gravity(float *g)
{
    g[0] = 2.0f * (s_qx * s_qz - s_qw * s_qy);
    g[1] = 2.0f * (s_qy * s_qz + s_qw * s_qx);
    g[2] = s_qw * s_qw - s_qx * s_qx - s_qy * s_qy + s_qz * s_qz;
}

/* 由实测重力方向构造「yaw = 0」的四元数：R = Ry(左倾) · Rx(前倾) */
static void ui_attitude_set_from_gravity(const float *a)
{
    float g[3];
    float pitch;
    float roll;
    float cp;
    float sp;
    float cr;
    float sr;

    g[0] = a[0];
    g[1] = a[1];
    g[2] = a[2];
    if (!ui_attitude_normalize(g))
    {
        s_qw = 1.0f;
        s_qx = 0.0f;
        s_qy = 0.0f;
        s_qz = 0.0f;
        return;
    }

    /* 前倾为正：绕 X 正转；左倾为正：绕 Y 正转 */
    pitch = atan2f(g[1], g[2]);
    roll = atan2f(-g[0], sqrtf(g[1] * g[1] + g[2] * g[2]));

    cp = cosf(0.5f * pitch);
    sp = sinf(0.5f * pitch);
    cr = cosf(0.5f * roll);
    sr = sinf(0.5f * roll);

    /* q = qy(roll) ⊗ qx(pitch) */
    s_qw = cr * cp;
    s_qx = cr * sp;
    s_qy = sr * cp;
    s_qz = 0.0f;
}

/* 读一帧 12B：陀螺 XYZ + 加速度 XYZ，小端 */
static bool ui_attitude_read(float *gyro, float *acc)
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

void UIAttitude_Init(void)
{
    /* 周期计数器：dt 精度到 1/168 MHz，卡顿和刷屏都能如实反映 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    s_qw = 1.0f;
    s_qx = 0.0f;
    s_qy = 0.0f;
    s_qz = 0.0f;
    s_bias[0] = 0.0f;
    s_bias[1] = 0.0f;
    s_bias[2] = 0.0f;
    s_calib_count = 0U;
    s_calib_retry = 0U;
    s_ready = false;
    s_last_cycle = DWT->CYCCNT;
    s_seen_seq = imu_seq;
}

void UIAttitude_CalibStart(void)
{
    float gyro[3];
    float acc[3] = {0.0f, 0.0f, 1.0f};

    s_ready = false;
    s_calib_count = 0U;
    s_calib_retry = 0U;
    s_calib_sum[0] = 0.0f;
    s_calib_sum[1] = 0.0f;
    s_calib_sum[2] = 0.0f;
    s_calib_acc[0] = 0.0f;
    s_calib_acc[1] = 0.0f;
    s_calib_acc[2] = 0.0f;

    /* 先按当前重力给出姿态：标定期间画面就能显示正确的初始姿态，而不是单位阵 */
    if (ui_attitude_read(gyro, acc))
    {
        ui_attitude_set_from_gravity(acc);
    }
    s_last_cycle = DWT->CYCCNT;
    s_seen_seq = imu_seq;
}

UI_ATTITUDE_CalibStatus UIAttitude_CalibStep(uint16_t samples)
{
    float gyro[3];
    float acc[3];
    float acc_mg;
    float gyro_sq;
    uint16_t n;
    uint8_t i;

    for (n = 0U; n < samples; ++n)
    {
        if (++s_calib_retry > (UI_ATTITUDE_CALIB_SAMPLES * 10U))
        {
            break;
        }
        HAL_Delay(1U);
        if (!ui_attitude_read(gyro, acc))
        {
            continue;
        }

        acc_mg = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]) *
                 UI_ATTITUDE_ACC_MG_PER_LSB;
        gyro_sq = gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2];
        if (acc_mg < 800.0f || acc_mg > 1200.0f ||
            gyro_sq > (float)(UI_ATTITUDE_GYRO_QUIET_LSB * UI_ATTITUDE_GYRO_QUIET_LSB))
        {
            /* 在动：整段丢弃，动画继续转 */
            s_calib_count = 0U;
            s_calib_sum[0] = 0.0f;
            s_calib_sum[1] = 0.0f;
            s_calib_sum[2] = 0.0f;
            s_calib_acc[0] = 0.0f;
            s_calib_acc[1] = 0.0f;
            s_calib_acc[2] = 0.0f;
            continue;
        }

        for (i = 0U; i < 3U; i++)
        {
            s_calib_sum[i] += gyro[i];
            s_calib_acc[i] += acc[i];
        }
        if (++s_calib_count >= UI_ATTITUDE_CALIB_SAMPLES)
        {
            for (i = 0U; i < 3U; i++)
            {
                s_bias[i] = s_calib_sum[i] / (float)s_calib_count;
                s_calib_acc[i] /= (float)s_calib_count;
            }
            /* 标定完成后按平均重力重置 tilt 与 yaw */
            ui_attitude_set_from_gravity(s_calib_acc);
            s_last_cycle = DWT->CYCCNT;
            s_seen_seq = imu_seq;
            s_ready = true;
            return UI_ATTITUDE_CALIB_DONE;
        }
    }

    if (s_calib_retry > (UI_ATTITUDE_CALIB_SAMPLES * 10U))
    {
        /* 一直在动或 IMU 没通：零偏保持 0，姿态沿用 CalibStart 的初始值 */
        s_bias[0] = 0.0f;
        s_bias[1] = 0.0f;
        s_bias[2] = 0.0f;
        s_last_cycle = DWT->CYCCNT;
        s_seen_seq = imu_seq;
        s_ready = true;
        return UI_ATTITUDE_CALIB_FAILED;
    }

    return UI_ATTITUDE_CALIB_BUSY;
}

void UIAttitude_ZeroYaw(void)
{
    float yw_x;
    float yw_y;
    float psi;
    float c;
    float s;

    if (!s_ready)
    {
        return;
    }

    /* 机体 Y（车尾）转到世界系 = R 的第二列；取它在水平面（X-Y）的方位角。
       方位角定义 psi = atan2(yw_x, yw_y)，绕 Z 转 theta 后变为 psi - theta，
       所以要让新朝向为零必须转 theta = +psi。 */
    yw_x = 2.0f * (s_qx * s_qy - s_qw * s_qz);
    yw_y = 1.0f - 2.0f * (s_qx * s_qx + s_qz * s_qz);

    /* 车立成 ±90° 时车尾方向接近垂直，水平方位角没有意义，此时不动 yaw */
    if (sqrtf(yw_x * yw_x + yw_y * yw_y) < 0.2f)
    {
        return;
    }

    psi = atan2f(yw_x, yw_y);

    /* q ← qz(+psi) ⊗ q；符号反了会让偏角翻倍而不是归零（按两次 = 4 倍） */
    c = cosf(0.5f * psi);
    s = sinf(0.5f * psi);
    {
        float w = c * s_qw - s * s_qz;
        float x = c * s_qx - s * s_qy;
        float y = c * s_qy + s * s_qx;
        float z = c * s_qz + s * s_qw;

        s_qw = w;
        s_qx = x;
        s_qy = y;
        s_qz = z;
    }
}

void UIAttitude_Update(void)
{
    float g_meas[3];
    float g_est[3];
    float e[3];
    float omega[3];
    float dq;
    float dt;
    float half_dt;
    float w;
    float x;
    float y;
    float z;
    uint32_t seq;

    /* 只消费 imu_service() 发布的新快照，不自己读 SPI：
       两个读者会让 SPI3 流量翻倍，而快照本来就已经是最新的。 */
    seq = imu_seq;
    if (seq == s_seen_seq)
    {
        return;
    }
    s_seen_seq = seq;

    dt = (float)(DWT->CYCCNT - s_last_cycle) / 168000000.0f;
    s_last_cycle = DWT->CYCCNT;
    if (dt <= 0.0f)
    {
        return;
    }
    if (dt > UI_ATTITUDE_DT_MAX_S)
    {
        return;     /* 卡顿过久：丢掉这一段，不硬积分 */
    }

    g_meas[0] = (float)imu_acc[0];
    g_meas[1] = (float)imu_acc[1];
    g_meas[2] = (float)imu_acc[2];
    if (!ui_attitude_normalize(g_meas))
    {
        return;
    }

    if (s_ready)
    {
        ui_attitude_gravity(g_est);

        /* 误差 = 实测 × 估计；符号决定修正方向（前倾时修正量绕 X 正向） */
        e[0] = g_meas[1] * g_est[2] - g_meas[2] * g_est[1];
        e[1] = g_meas[2] * g_est[0] - g_meas[0] * g_est[2];
        e[2] = g_meas[0] * g_est[1] - g_meas[1] * g_est[0];
    }
    else
    {
        e[0] = 0.0f;
        e[1] = 0.0f;
        e[2] = 0.0f;
    }

    omega[0] = ((float)imu_gyro[0] - s_bias[0]) * UI_ATTITUDE_GYRO_LSB_TO_RADS + UI_ATTITUDE_KP * e[0];
    omega[1] = ((float)imu_gyro[1] - s_bias[1]) * UI_ATTITUDE_GYRO_LSB_TO_RADS + UI_ATTITUDE_KP * e[1];
    omega[2] = ((float)imu_gyro[2] - s_bias[2]) * UI_ATTITUDE_GYRO_LSB_TO_RADS + UI_ATTITUDE_KP * e[2];

    /* q̇ = 0.5 · q ⊗ ω */
    half_dt = 0.5f * dt;
    w = s_qw;
    x = s_qx;
    y = s_qy;
    z = s_qz;

    dq = (0.0f
          - x * omega[0] - y * omega[1] - z * omega[2]) * half_dt;
    s_qw += dq;
    dq = (w * omega[0] + y * omega[2] - z * omega[1]) * half_dt;
    s_qx += dq;
    dq = (w * omega[1] - x * omega[2] + z * omega[0]) * half_dt;
    s_qy += dq;
    dq = (w * omega[2] + x * omega[1] - y * omega[0]) * half_dt;
    s_qz += dq;

    /* 归一化，防止长时间积分发散 */
    {
        float n = sqrtf(s_qw * s_qw + s_qx * s_qx + s_qy * s_qy + s_qz * s_qz);

        if (n > 1.0e-6f)
        {
            s_qw /= n;
            s_qx /= n;
            s_qy /= n;
            s_qz /= n;
        }
        else
        {
            s_qw = 1.0f;
            s_qx = 0.0f;
            s_qy = 0.0f;
            s_qz = 0.0f;
        }
    }
}

void UIAttitude_GetMatrix(float *R)
{
    float w = s_qw;
    float x = s_qx;
    float y = s_qy;
    float z = s_qz;

    if (R == NULL)
    {
        return;
    }

    /* 体 → 世界，行优先 */
    R[0] = 1.0f - 2.0f * (y * y + z * z);
    R[1] = 2.0f * (x * y - w * z);
    R[2] = 2.0f * (x * z + w * y);

    R[3] = 2.0f * (x * y + w * z);
    R[4] = 1.0f - 2.0f * (x * x + z * z);
    R[5] = 2.0f * (y * z - w * x);

    R[6] = 2.0f * (x * z - w * y);
    R[7] = 2.0f * (y * z + w * x);
    R[8] = 1.0f - 2.0f * (x * x + y * y);
}

bool UIAttitude_IsReady(void)
{
    return s_ready;
}

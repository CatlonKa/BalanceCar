#ifndef __ATTITUDE_H__
#define __ATTITUDE_H__



/*
YAW  偏航
偏航角：右转为正，左转为负

PITCH 俯仰
俯仰角：前倾为正，后仰为负

ROLL  横滚
横滚角：右倾为正，左倾为负
*/
#include <stdbool.h>
#include <stdint.h>

/* 调用周期：由 TIM5 1 kHz 中断驱动，固定 1 ms */
#define ATTITUDE_DT_S                0.001f

/* 互补滤波系数：fc ≈ (1-α)/(2π·α·dt)，dt=1ms、α=0.98 → 3.2 Hz */
#define ATTITUDE_ALPHA               0.98f

/* 陀螺侧：零偏标定样本数（500 × 1ms = 0.5s） */
#define ATTITUDE_GYRO_CALIB_SAMPLES    500U
#define ATTITUDE_GYRO_CALIB_MAX_LSB    60     /* 约 1 dps，三轴合成超此值视为在动 */

/* 加速度侧：静止判据，只看矢量模长是否接近 1 g */
#define ATTITUDE_ACC_CALIB_MIN_MG      800    /* 0.8 g，低于视为自由落体 */
#define ATTITUDE_ACC_CALIB_MAX_MG      1200   /* 1.2 g，高于视为受冲击 */

/* 轴映射：角速度取陀螺 PITCH 轴，倾角由加速度另两轴比值算 */
#define ATTITUDE_GYRO_PITCH_AXIS       0U     /* 0=X 1=Y 2=Z */
#define ATTITUDE_ACC_TILT_NUM_AXIS     1U     /* 分子轴 */
#define ATTITUDE_ACC_TILT_DEN_AXIS     2U     /* 分母轴 */

/* 方向修正：实测方向反了就改成 -1.0f，不要改轴映射 */
#define ATTITUDE_ACC_PITCH_SIGN       (+1.0f)
#define ATTITUDE_GYRO_RATE_SIGN       (+1.0f)

/* 零偏标定 + 起算角初始化。**阻塞约 0.5 s**，必须在 imu_init() 之后调。
   车要保持静止，否则会一直重试（最多 5 s）；标定失败时 IsCalibrating() 保持 true。 */
void Attitude_Init(void);

/* 每 1 ms 调一次：读 imu_gyro/imu_acc 快照更新姿态。标定不在这里做。 */
void Attitude_Update(void);

/* 重新标定（同样阻塞约 0.5 s）。车必须静止；建议挂在停车分支。 */
void Attitude_Calibrate(void);

/* true = 标定未完成或失败；此时 GetPitch/GetPitchRate 恒返回 0。
   IMU 没通时永远为 true —— 可以直接拿它做「禁止驱动」的联锁。 */
bool Attitude_IsCalibrating(void);

/*
 * 车是否还没倒：只用于识别「已倒下」，正常摆动不切断控制。
 *
 * 除了 |pitch| 在窗口内，还要求重力确实落在 pitch 平面里：
 * 侧躺 90° 时 acc[NUM]、acc[DEN] 都趋 0，单轴 pitch 会误报 0（「完美直立」），
 * 只靠 GetPitch() 分不出「直立」和「侧躺」，必须用本函数。
 * 判据：|pitch| ≤ 60° 且 重力平面内占比 ≥ 0.5（约 60° 以内侧倾）。
 */
bool Attitude_IsUpright(void);

/* 俯仰角，rad，前倾为正 */
float Attitude_GetPitch(void);

/* 俯仰角速度，rad/s */
float Attitude_GetPitchRate(void);

#endif

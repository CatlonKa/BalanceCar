#ifndef __ATTITUDE_H__
#define __ATTITUDE_H__

#include <stdbool.h>
#include <stdint.h>

/* 调用周期：由 TIM5 1 kHz 中断驱动，固定 1 ms */
#define ATTITUDE_DT_S                0.001f

/* 互补滤波系数：fc ≈ α/(2π(1-α)dt)，dt=1ms 下 0.98 → 约 3.2 Hz */
#define ATTITUDE_ALPHA               0.98f

/* 陀螺零偏标定：连续静止样本数（500 × 1ms = 0.5s）与静止判据 */
#define ATTITUDE_CALIB_SAMPLES       500U
#define ATTITUDE_CALIB_GYRO_MAX_LSB  60     /* 约 1 dps，三轴合成超此值视为在动 */
#define ATTITUDE_CALIB_ACC_MIN_MG    800    /* 0.8 g，低于视为自由落体 */
#define ATTITUDE_CALIB_ACC_MAX_MG    1200   /* 1.2 g，高于视为受冲击 */

/* 轴映射：俯仰 = 绕 PITCH 轴旋转，角速度取该轴陀螺；倾角由另两轴 LSB 比值算 */
#define ATTITUDE_PITCH_AXIS          0U     /* 0=X 1=Y 2=Z */
#define ATTITUDE_TILT_NUM_AXIS       1U     /* 分子轴 */
#define ATTITUDE_TILT_DEN_AXIS       2U     /* 分母轴 */

/* 方向修正：实测方向反了就改成 -1.0f，不要改轴映射 */
#define ATTITUDE_PITCH_SIGN         (+1.0f)
#define ATTITUDE_RATE_SIGN          (+1.0f)

void Attitude_Init(void);

/* 每 1 ms 调一次：读 imu_gyro/imu_acc 快照，标定或更新姿态 */
void Attitude_Update(void);

/* 重新开始零偏标定（车必须静止，未静止会自动重来） */
void Attitude_Calibrate(void);

/* 标定是否仍在进行；此时 GetPitch/GetPitchRate 恒返回 0 */
bool Attitude_IsCalibrating(void);

/* 俯仰角，rad，前倾为正 */
float Attitude_GetPitch(void);

/* 俯仰角速度，rad/s */
float Attitude_GetPitchRate(void);

#endif

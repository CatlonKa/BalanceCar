#ifndef __UI_ATTITUDE_H__
#define __UI_ATTITUDE_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * 界面用三维姿态（四元数），与控制用的 Attitude 完全独立。
 *
 * 为什么单独做一份：
 *   - 控制那份跑在 1 kHz 中断里、只算单轴、要求低延迟
 *   - 这份要 360° 全姿态、按需更新、可以阻塞标定，只在界面页活跃时工作
 *
 * 算法：陀螺积分 + 加速度计修正 tilt（Mahony），不做 yaw 修正（无磁力计）。
 *   yaw 只靠陀螺，所以会漂；由 UIAttitude_ZeroYaw() 手动归零。
 *
 * 坐标（已实测）：X = 车左侧，Y = 车尾，Z = 上。右手系。
 *   前倾（车头下压）= 绕 X 正转，pitch = atan2(a_y, a_z)
 *   右倾 = 绕 Y 负转，所以这里统一用「绕 Y 右手正转」的角，左倾为正
 */

/* 陀螺 ±500 dps → rad/s 每 LSB */
#define UI_ATTITUDE_GYRO_LSB_TO_RADS   3.054326e-4f

/* 加速度 ±4 g → mg 每 LSB，只用于静止判据 */
#define UI_ATTITUDE_ACC_MG_PER_LSB     0.122f

/* 陀螺静止判据：三轴合成超此值视为在动（60 LSB ≈ 1 dps） */
#define UI_ATTITUDE_GYRO_QUIET_LSB     60

/* acc 修正增益，单位 1/s：越大收敛越快，但手抖动带来的线性加速度也越容易灌进来 */
#define UI_ATTITUDE_KP                 0.6f

/* 标定取多少个样本（1 个 ≈ 1 ms，200 ≈ 0.2 s） */
#define UI_ATTITUDE_CALIB_SAMPLES      200U

/* 单次积分的时间上限，防卡顿后一次转一大截 */
#define UI_ATTITUDE_DT_MAX_S           0.05f

/* 标定状态：BUSY 表示还在采样本，界面应显示进度动画 */
typedef enum
{
    UI_ATTITUDE_CALIB_BUSY = 0,     /* 还在采样本 */
    UI_ATTITUDE_CALIB_DONE,         /* 成功 */
    UI_ATTITUDE_CALIB_FAILED        /* 超时或 IMU 没通，已按兜底姿态起步 */
} UI_ATTITUDE_CalibStatus;

/* 启用 DWT 周期计数器（用于精确 dt）；只需在 main() 里调一次 */
void UIAttitude_Init(void);

/*
 * 开始标定（非阻塞）。立即按当前重力给出姿态，所以画面一开始就是正确的初始姿态。
 * 之后由 UIAttitude_CalibStep() 分批采样本。
 */
void UIAttitude_CalibStart(void);

/*
 * 推进一步标定：采 samples 个静止样本（每个间隔 1 ms，阻塞 samples 毫秒）。
 * 返回 BUSY 表示还没完，界面可以换一帧动画再调一次。
 * samples 取 10~25：太小则标定拖得久，太大则动画卡顿。
 */
UI_ATTITUDE_CalibStatus UIAttitude_CalibStep(uint16_t samples);

/* yaw 归零：保留当前 tilt，把水平朝向设为参考（0） */
void UIAttitude_ZeroYaw(void);

/* 每轮调用一次；内部按 DRDY 判断是否有新样本，没有就立即返回 */
void UIAttitude_Update(void);

/* 取旋转矩阵（体 → 世界，行优先 R[9]）；未初始化返回单位阵 */
void UIAttitude_GetMatrix(float *R);

/* 是否已完成标定（Reset 成功过） */
bool UIAttitude_IsReady(void);

#endif

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

/* 互补滤波系数：fc = (1-α)/(2π·α·dt)，**α 与 dt 强绑定**，且必须远低于内环 ωn。

   约束：fc ≤ ωn/3，否则加计通路（带一阶滞后）进入环路带宽 → 吃相位裕度。
   ωn = √(0.05·KP − 98)：KP=3000 → 1.15 Hz；KP=6000 → 2.26 Hz。
   所以 α 不能单独调，它跟着 KP 走（见 docs/直立环修复-执行顺序.md）。

   0.998 → fc 0.32 Hz、τ 0.5 s、加计权重 0.2%/ms。
   ❗实测（2026-11-16）：0.99（fc 1.6 Hz > ωn 1.15 Hz）→ 低频抖动加剧、更容易冲出去。 */
#define ATTITUDE_ALPHA               0.998f

/* 陀螺侧：零偏标定样本数（500 × 1ms = 0.5s） */
#define ATTITUDE_GYRO_CALIB_SAMPLES    500U
#define ATTITUDE_GYRO_CALIB_MAX_LSB    60     /* 约 1 dps，三轴合成超此值视为在动 */

/* 加速度侧：静止判据，只看矢量模长是否接近 1 g */
#define ATTITUDE_ACC_CALIB_MIN_MG      800    /* 0.8 g，低于视为自由落体 */
#define ATTITUDE_ACC_CALIB_MAX_MG      1200   /* 1.2 g，高于视为受冲击 */

/* ---- 线加速度处理 ----
   加速时 acc[1] = g·sinθ − a·cosθ，代入 a = g·tan(θ−θ₀) 得 acc_tilt ≡ θ₀（与 a 无关）：
   运动时加计把 pitch 钉在机械中值上，抹掉 lean 信息 θ−θ₀。
   详见 docs/姿态估计-线性加速度补偿.md */

/* 阈值量纲换算：k_x = 226080 counts/m ⇒ 1 m/s² = 452 (计数/2ms)/s */

/* 自适应门控（备选）：检出线加速度就把权重交给陀螺，α_eff = 1 − (1−α)·w，
   w 在 [A_LO, A_HI] 线性从 1 降到 0。只在 LIN_ACC 扣补失效时改用。
   静止时 a=0 → w=1 → α_eff 精确等于 ATTITUDE_ALPHA，不改变静立行为。
   0 = 关闭；开启时 A_LO 不低于 200（a 噪声底约 50，取 4σ） */
#define ATTITUDE_ACC_GATE_ENABLE       0
#define ATTITUDE_ACC_GATE_A_LO         150.0f /* ≈0.33 m/s² = 1.9° 污染，低于此完全信任加计 */
#define ATTITUDE_ACC_GATE_A_HI         600.0f /* ≈1.33 m/s² = 7.7° 污染，高于此纯陀螺积分 */

/* a 估计的一阶低通系数 @1kHz，τ ≈ dt/α = 50 ms */
#define ATTITUDE_LIN_ACC_ALPHA         0.02f

/* 精确扣补：y = f_Y + a·cosθ、z = f_Z − a·sinθ，扣掉后 acc_tilt 恢复等于真实 pitch。
   a 由编码器微分估计，残余噪声约 0.45° 进 pitch（×KP3000 = 21 PWM）。 */
#define ATTITUDE_LIN_ACC_ENABLE        1
#define ATTITUDE_LIN_ACC_GAIN          1.848f  /* LSB 每 (计数/2ms)/s = 835.8/452.2 = 417919/k_x，正号已实测确认 */
#define ATTITUDE_LIN_ACC_MAX_LSB       1600.0f /* 0.19 g ≈ 11°，覆盖 pitch_set ±0.12 全量程 */

/* 轴映射：角速度取陀螺 PITCH 轴，倾角由加速度另两轴比值算 */
#define ATTITUDE_GYRO_PITCH_AXIS       0U     /* 0=X 1=Y 2=Z */
#define ATTITUDE_GYRO_YAW_AXIS         2U     /* 偏航轴 */
#define ATTITUDE_ACC_TILT_NUM_AXIS     1U     /* 分子轴 */
#define ATTITUDE_ACC_TILT_DEN_AXIS     2U     /* 分母轴 */

/* 方向修正：实测方向反了就改成 -1.0f，不要改轴映射 */
#define ATTITUDE_ACC_PITCH_SIGN       (+1.0f)
#define ATTITUDE_GYRO_RATE_SIGN       (+1.0f)
#define ATTITUDE_GYRO_YAW_SIGN        (-1.0f)

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

/* 偏航角速度，rad/s，右转为正（符号待实测确认）。标定未完成时恒 0。 */
float Attitude_GetYawRate(void);

/* 加速度计单独算的倾角，rad；仅供遥测与补偿对比，不进控制 */
float Attitude_GetAccTilt(void);

#endif

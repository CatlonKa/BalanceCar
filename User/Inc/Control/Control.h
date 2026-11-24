#ifndef __CONTROL_H__
#define __CONTROL_H__

#include <stdint.h>

/* 控制周期（TIM5 1kHz）；速度量 2ms 刷新一次，零阶保持 */
#define CONTROL_DT_S            0.001f

/* 输出限幅，单位 = PWM 计数（TIM2 ARR = 1000） */
#define CONTROL_PWM_MAX         999

/* 静摩擦前馈：地面实测起转 50 计数（悬空 40，地面阻力约 10） */
#define CONTROL_DEADZONE        0

/* 试跑：1 = 忽略 Control_SetTarget，改用下面的固定目标量。上线前改回 0。
   ⚠️ car_run 上电即 1，所以标定一完成车就会前冲，上电前先把车扶稳 */
#define CONTROL_TEST_ENABLE     0
#define CONTROL_TEST_SPEED      60.0f    
#define CONTROL_TEST_STEER      0.0f    /* PWM 计数，|值|>50 才推得动 */

/* 控制方式：自动巡线 / 手动遥控。手动输入还没接，切过去就不驱动 */
#define CONTROL_MODE_AUTO       0
#define CONTROL_MODE_MANUAL     1

/* 上电默认值：界面开关初值也从这里取 */
#define CONTROL_ALGO_DEFAULT    CONTROL_ALGO_PID
#define CONTROL_MODE_DEFAULT    CONTROL_MODE_AUTO

/* 观测量与目标量：观测由 Control_Update 采集，目标由 Control_SetTarget 下 */
typedef struct
{
    float pitch;            /* rad，前倾为正 */
    float pitch_rate;       /* rad/s */
    float speed;            /* 计数/周期，左右均值，前进为正 */
    float turn_rate;        /* 计数/周期，左快为正（轮速差，积分即角度） */
    float speed_left;       /* 计数/周期 */
    float speed_right;      /* 计数/周期 */
    float yaw_rate;         /* rad/s，陀螺 Z 轴，只扣零偏不积分 */
    float target_speed;     /* 计数/周期，上层给 */
    float target_yaw_rate;  /* rad/s，上层给（闭环转向用） */
    float target_steer;     /* PWM 计数，上层给（开环转向用，非 0 优先） */
    float dt;               /* s */
} Control_Input;

/* 期望占空比，-CONTROL_PWM_MAX ~ +CONTROL_PWM_MAX */
typedef struct
{
    float left;
    float right;
} Control_Output;

/* 一种算法：切入时 reset，之后每周期 step */
typedef struct
{
    const char *name;
    void (*reset)(void);
    void (*step)(const Control_Input *in, Control_Output *out);
} Control_Algo;

/* 算法索引，与 Control.c 的 s_algos[] 顺序一致 */
enum
{
    CONTROL_ALGO_PID = 0,
    CONTROL_ALGO_LQR,
    CONTROL_ALGO_COUNT
};

void Control_Init(void);
void Control_Reset(void);
void Control_Update(void);
void Control_SetAlgo(uint8_t idx);
uint8_t Control_GetAlgo(void);
void Control_SetMode(uint8_t mode);
uint8_t Control_GetMode(void);

/* 下目标量：每周期调一次（TIM7，Control_Update 之前）。不调则沿用上次值。
   steer 非 0 时走开环差速并忽略 yaw_rate，避免两条转向回路打架 */
void Control_SetTarget(float speed, float yaw_rate, float steer);

/* 上拍共模 PWM（推力分量）；仅供遥测，不参与控制 */
int16_t Control_GetPwmCommon(void);

#endif

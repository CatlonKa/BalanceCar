#ifndef __CONTROL_H__
#define __CONTROL_H__

#include <stdint.h>

/* 控制周期（TIM5 1kHz）；速度量 2ms 刷新一次，零阶保持 */
#define CONTROL_DT_S            0.001f

/* 输出限幅，单位 = PWM 计数（TIM2 ARR = 1000） */
#define CONTROL_PWM_MAX         999

/* 控制方式：自动巡线 / 手动遥控。手动输入还没接，切过去就不驱动 */
#define CONTROL_MODE_AUTO       0
#define CONTROL_MODE_MANUAL     1

/* 上电默认值：界面开关初值也从这里取 */
#define CONTROL_ALGO_DEFAULT    CONTROL_ALGO_PID
#define CONTROL_MODE_DEFAULT    CONTROL_MODE_AUTO

/* 观测量：本周期快照，由 Control_Update 采集 */
typedef struct
{
    float pitch;            /* rad，前倾为正 */
    float pitch_rate;       /* rad/s */
    float speed;            /* 计数/周期，左右均值，前进为正 */
    float turn_rate;        /* 计数/周期，左快为正 */
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

#endif

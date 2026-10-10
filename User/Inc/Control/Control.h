#ifndef __CONTROL_H__
#define __CONTROL_H__

#include <stdint.h>

/* 速度采样周期（TIM6 500Hz）：数据 2ms 更新一次 */
#define CONTROL_DT_S            0.002f

/* 输出限幅，单位 = PWM 计数（TIM2 ARR = 1000） */
#define CONTROL_PWM_MAX         999

/* 姿态是否已接入：Attitude 未实现前置 0，pitch/pitch_rate 恒为 0 */
#define CONTROL_HAVE_ATTITUDE   0

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

#endif

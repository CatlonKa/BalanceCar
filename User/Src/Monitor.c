#include "Monitor.h"

#include "main.h"
#include "Attitude.h"
#include "Control.h"
#include "Control_PID.h"
#include "event.h"

#if (MONITOR_OUT_UART)
#include "uart.h"
#endif

#if (MONITOR_OUT_RTT)
#include "Rtt.h"
#endif

#if (MONITOR_OUT_RADIO)
#include "Telemetry.h"
#endif

static uint32_t s_last_tick;

/* 6 路快照，顺序即各出口帧内顺序；调整路数要同步改 MONITOR_FIELDS */
static void monitor_collect(float *f)
{
    f[0] = Attitude_GetPitch();               /* rad，前倾为正 */
    f[1] = Attitude_GetPitchRate();           /* rad/s */
    f[2] = 0.5f * (float)(speedL + speedR);   /* 计数/2ms，前进为正 */
    f[3] = Control_PID_GetPitchSet();         /* rad，直立环目标角 */
    f[4] = (float)Control_GetPwmCommon();     /* PWM 计数，共模输出 */
    f[5] = Attitude_GetAccTilt();             /* rad，加速度计独算的倾角 */
}

void Monitor_Init(void)
{
#if (MONITOR_OUT_RTT)
    Rtt_Init();
#endif

#if (MONITOR_OUT_RADIO)
    Telemetry_Init();
#endif

    s_last_tick = HAL_GetTick();
}

void Monitor_Update(void)
{
    float f[MONITOR_FIELDS];
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - s_last_tick) < (1000U / MONITOR_HZ))
    {
        return;
    }
    s_last_tick = now;

    monitor_collect(f);

    /* 上电标定失败后 s_gyro_calibrating 恒 true，姿态输出恒 0、车永远不动。
       停车静止时重试（不能放中断里：加计标定内部 HAL_Delay 依赖 SysTick）。 */
    if (car_run == 0U && Attitude_IsCalibrating())
    {
        Attitude_Calibrate();
    }

#if (MONITOR_OUT_UART)
    /* 上一帧没发完就丢弃本帧，不等待 */
    (void)uart_send_floats(f, MONITOR_FIELDS);
#endif

#if (MONITOR_OUT_RTT)
    Rtt_SendFloats(f, MONITOR_FIELDS);
#endif

#if (MONITOR_OUT_RADIO)
    (void)Telemetry_Send(f, MONITOR_FIELDS);
#endif
}

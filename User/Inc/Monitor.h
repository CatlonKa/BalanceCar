#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <stdint.h>

/*
 * 观测层：采集系统状态快照，按开关分发到各出口。
 * 出口互相独立可同时开启，同一拍数据一起给到每个出口。
 */

/* 出口开关：1 = 参与编译，可同时为 1 */
#define MONITOR_OUT_UART        1   /* USART3 -> VOFA+ JustFloat */
#define MONITOR_OUT_RTT         0   /* SEGGER RTT ch1，需 J-Link */
#define MONITOR_OUT_RADIO       0   /* SPI2 无线遥测，见 Telemetry.h */

/* 快照路数，顺序见 Monitor.c 的 monitor_collect() */
#define MONITOR_FIELDS          6U

/* 采样率 Hz。6 路 = 28 B/帧，115200 下 2.43 ms，300 Hz 占空 73%。
   100 Hz 时 10~30 Hz 抖动每周期只有 3 个点，看不出真实波形。 */
#define MONITOR_HZ              300U

/* 上电调一次：按需初始化 RTT 缓冲与射频 */
void Monitor_Init(void);

/* 主循环每轮调一次，内部按 MONITOR_HZ 限频；禁止在中断里调用 */
void Monitor_Update(void);

#endif

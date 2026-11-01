#ifndef __RTT_H__
#define __RTT_H__

#include <stdint.h>

/*
 * SEGGER RTT 遥测（J-Link 专用，不需要额外引脚）
 *
 * 通道分配：
 *   ch0 文本 —— Rtt_SendText()
 *   ch1 二进制 —— Rtt_SendFloats()，接 cortex-debug 的 binary/float 解码器
 *
 * 为什么不用 SEGGER_RTT_printf：它会把 vsnprintf 拉进来（约 3.4 KB Flash），
 * 与本工程「不用 printf」的既定做法冲突。要格式化请用整数手拼。
 *
 * ⚠️ 前提：固件必须链接本模块，否则调试器找不到 _SEGGER_RTT 控制块，
 *    rttConfig 会一直重试失败。
 */

/* 遥测采样率，Hz。
   上限由 HAL_GetTick 的 1 ms 分辨率决定；实测 500 kHz SWD 可稳定跑 1700 Hz，
   1000 Hz 与 TIM7 控制环同频，留有余量。 */
#define RTT_TELEMETRY_HZ        1000U

/* 通道 1（二进制波形）的上行缓冲大小，字节。
   ⚠️ 必须是单帧（20 B）的整数倍：否则帧会跨环形回绕边界被拆成两段，
      离线 dump 解析时会错位。
   4000 B = 200 帧，兼作「黑盒」：主机卡顿时不丢，事后可直接 dump 这 200 ms 历史。 */
#define RTT_CH1_BUFFER_SIZE     4000U

/* 一帧的通道数，顺序与 Rtt_Telemetry() 里填入的字段一致 */
#define RTT_TELEMETRY_CH_COUNT  5U

void Rtt_Init(void);

/* 每轮主循环调用一次；内部按 RTT_TELEMETRY_HZ 限频，没到点立即返回 */
void Rtt_Telemetry(void);

/* 往 ch0 发一段文本（不加换行）。传 NULL 或空串直接返回。 */
void Rtt_SendText(const char *text);

/* 往 ch1 发 n 个 float（小端，无帧尾）。n 上限 RTT_TELEMETRY_CH_COUNT */
void Rtt_SendFloats(const float *values, uint32_t count);

#endif

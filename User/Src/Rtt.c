#include "Rtt.h"
#include "SEGGER_RTT.h"
#include "Attitude.h"
#include "event.h"
#include "Analog.h"

#include <string.h>

/* ch0 文本、ch1 二进制 float；全部 NO_BLOCK_SKIP，缓冲满即丢，不阻塞 */

/* 限频用：上次发送时刻（ms） */
static uint32_t s_last_tick;

/* 通道 1 的上行缓冲。必须自己声明并传进去：
   SEGGER_RTT_ConfigUpBuffer() 传 NULL/0 的语义是「沿用已有缓冲」，
   而通道 1 默认没有缓冲 → 写入会被静默丢弃（WrOff 在动但数据不落地）。 */
static char s_ch1_buffer[RTT_CH1_BUFFER_SIZE];

void Rtt_Init(void)
{
    /* SEGGER_RTT.c 用静态初始化建好控制块与 ch0 缓冲，
       这里只需给 ch1 挂上真实缓冲并起名，便于调试器识别。 */
    (void)SEGGER_RTT_ConfigUpBuffer(1U, "Float",
                                    s_ch1_buffer, sizeof(s_ch1_buffer),
                                    SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    s_last_tick = HAL_GetTick();
}

void Rtt_SendText(const char *text)
{
    if (text == NULL)
    {
        return;
    }
    if (text[0] == '\0')
    {
        return;
    }
    (void)SEGGER_RTT_WriteString(0U, text);
}

void Rtt_SendFloats(const float *values, uint32_t count)
{
    if (values == NULL || count == 0U)
    {
        return;
    }
    if (count > RTT_TELEMETRY_CH_COUNT)
    {
        count = RTT_TELEMETRY_CH_COUNT;
    }
    /* 小端机 + IEEE754，直接按字节写即是解码器要的格式 */
    (void)SEGGER_RTT_Write(1U, values, count * (uint32_t)sizeof(float));
}

void Rtt_Telemetry(void)
{
    float frame[RTT_TELEMETRY_CH_COUNT];
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - s_last_tick) < (1000U / RTT_TELEMETRY_HZ))
    {
        return;
    }
    s_last_tick = now;

    /* 顺序必须与 Rtt.h 的说明一致，改这里要同步改文档 */
    frame[0] = Attitude_GetPitch();
    frame[1] = Attitude_GetPitchRate();
    frame[2] = (float)speedL;
    frame[3] = (float)speedR;
    frame[4] = Analog_ReadVoltage();

    Rtt_SendFloats(frame, RTT_TELEMETRY_CH_COUNT);
}

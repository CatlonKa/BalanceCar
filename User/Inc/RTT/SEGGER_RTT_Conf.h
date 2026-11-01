#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

/*********************************************************************
*                    SEGGER RTT 本工程配置
*
* 只配两件事：缓冲区大小与通道数。
* 其余保持 SEGGER_RTT.c 里的默认值。
*
* 内存账（按当前配置）：
*   控制块 ~ 100 B + 上行 2 通道 × 256 B + 下行 1 通道 × 16 B ≈ 630 B
*
**********************************************************************
*/

/* 上行缓冲区（目标 → 主机）。通道 0 = 文本日志，通道 1 = 二进制波形。
   每个通道各占 BUFFER_SIZE_UP 字节，所以这里翻倍计。
   256 B 在 1 MHz SWD 下约 25 ms 读空；太小会丢帧，太大占 RAM。 */
#define BUFFER_SIZE_UP                                  256

/* 下行缓冲区（主机 → 目标）。本工程只发不收，16 B 足够。 */
#define BUFFER_SIZE_DOWN                                16

/* 上行通道数：0 = 文本，1 = 二进制帧 */
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS                   2

/* 下行通道数：只留 0（供 SEGGER_RTT_ConfigUpBuffer 的对称性，不实际使用） */
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS                 1

/* 默认模式：缓冲区满就丢弃，绝不阻塞。
   ⚠️ 这条是本工程能在 1 kHz 中断里安全调用的前提。
   BLOCK_IF_FIFO_FULL 会让写不进去时死等，等于把中断挂住。 */
#define SEGGER_RTT_MODE_DEFAULT                         SEGGER_RTT_MODE_NO_BLOCK_SKIP

/* 不启用 CPU cache 行对齐（Cortex-M4 无 D-Cache） */
#define SEGGER_RTT_CPU_CACHE_LINE_SIZE                  0

/* 不上锁：本工程没有 RTOS，写入点分散在中断与主循环，
   但 RTT 的 NO_BLOCK_SKIP 模式下指针更新是单字原子写，
   Cortex-M4 上不需要额外互斥。 */
#define SEGGER_RTT_NO_LOCK_INT_ENABLE                   1

#endif

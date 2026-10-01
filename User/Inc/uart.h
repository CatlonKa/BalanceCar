#ifndef __UART_H__
#define __UART_H__

#include "main.h"
#include "usart.h"

#include <stdint.h>

/*
 * USART3 调试串口。
 *
 * 硬件：PB10 = TX、PB11 = RX，115200 8N1，参数由 MX_USART3_UART_Init() 配置，
 * main() 里已经调用过，所以正常情况下不必再调 uart_init()。
 *
 * 这几个发送函数都是「阻塞发送 + 固定超时」：调用会一直等到最后一个字节进到
 * 发送移位寄存器才返回。在 115200 下大约每字节 87 us，用来打调试信息足够，
 * 但不要在中断里发长字符串。
 *
 * 失败（例如串口没初始化）只是把返回值丢掉，不会卡死在串口上。
 */

/** 发送一个字节。 */
void uart_send_byte(uint8_t byte);

/** 发送 length 个字节的原始数据，可以带任意二进制内容。 */
void uart_send(const uint8_t *data, uint16_t length);

/** 发送一个以 '\0' 结尾的字符串（不含结尾的 '\0'）。 */
void uart_send_string(const char *text);

/**
 * 格式化发送，用法与 printf 相同，例如 uart_printf("v=%u mV\r\n", mv)。
 * 返回实际写入缓冲区的字符数（被截断时返回截断后的长度），format 为 NULL 时返回 0。
 * 单条消息超过内部 128 字节缓冲区会被截断。
 *
 * 内部用 vsnprintf 组合成一条再发，所以不会出现多个任务/中断交错打断同一行的情况。
 *
 * 注意：本工程链接的是 newlib-nano，它的 printf 不带浮点格式化，
 * 所以 **不要用 %f / %e / %g**（不会输出数字）。要打电压用整数毫伏，
 * 例如 uart_printf("VBUS=%u.%03u V\r\n", mv / 1000U, mv % 1000U)，
 * 或者用下面的 uart_send_voltage()。
 */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
int uart_printf(const char *format, ...);

/**
 * 把一个毫伏值按「伏」发送，固定 3 位小数，例如 12345 -> "12.345"。
 * 纯整数运算，不依赖浮点 printf。
 *
 * 只发数字，不带单位也不带换行，方便拼进别的消息：
 *     uart_send_string("VBUS=");
 *     uart_send_voltage(mv);
 *     uart_send_string("V\r\n");
 * 或者只想要一行现成的，直接：
 *     uart_printf("VBUS=%u.%03u V\r\n", mv / 1000U, mv % 1000U);
 */
void uart_send_voltage(uint32_t millivolts);

#endif
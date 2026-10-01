#include "uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * USART3 调试串口。本文件只负责「把数据送出去」，不涉及协议。
 *
 * 之所以直接用 huart3：这块板子的 USART3 就是调试口（PB10/PB11），
 * 没有第二个用途，做成可切换通道反而要多传一个参数。
 */

/** 单次发送的阻塞超时，毫秒。 */
#define UART_TX_TIMEOUT_MS 10

/** uart_printf 的栈上缓冲区大小，超出部分会被截断。 */
#define UART_PRINTF_BUFFER 128


void uart_send_byte(uint8_t byte)
{
    HAL_UART_Transmit(&huart3, &byte, 1U, UART_TX_TIMEOUT_MS);
}

void uart_send(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0U) {
        return;
    }
    /* 发送期间只是读源数据，不修改它；HAL 的形参没有 const 才需要这一步转换。 */
    HAL_UART_Transmit(&huart3, (uint8_t *)(uintptr_t)data, length,
                            UART_TX_TIMEOUT_MS);
}

void uart_send_string(const char *text)
{
    if (text == NULL) {
        return;
    }
    uart_send((const uint8_t *)text, (uint16_t)strlen(text));
}

int uart_printf(const char *format, ...)
{
    char buffer[UART_PRINTF_BUFFER];
    va_list args;
    int length;

    if (format == NULL) {
        return 0;
    }

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length <= 0) {
        return length;
    }
    /* vsnprintf 返回的是「本来想写多少」，超长时按实际能发出的部分发。 */
    if ((size_t)length >= sizeof(buffer)) {
        length = (int)(sizeof(buffer) - 1U);
    }
    uart_send((const uint8_t *)buffer, (uint16_t)length);
    return length;
}

/*
 * newlib 的 printf 最终会走到 _write()，而 _write() 在 syscalls.c 里是弱定义、
 * 内部调用同样是弱定义的 __io_putchar()。这里给出强定义，printf 就直接从
 * USART3 出来了，例如 printf("speed=%d\r\n", speed)。
 *
 * 单字符一次传输，比 uart_printf 多几次 HAL 调用开销，但 UART 本身是阻塞发送、
 * 瓶颈在波特率上，实际差别可以忽略；要发长字符串时用 uart_printf 更省事。
 */
int __io_putchar(int ch)
{
    uart_send_byte((uint8_t)ch);
    return ch;
}

void uart_send_voltage(uint32_t millivolts)
{
    /*
     * 整数运算拼出「伏.毫伏」三位小数。
     * 本工程没有打开 newlib-nano 的浮点 printf（会多占十几 KB Flash），
     * 所以电压一律走这条路径，不要用 %f。
     */
    (void)uart_printf("%u.%03uV",
                      (unsigned)(millivolts / 1000U),
                      (unsigned)(millivolts % 1000U));
}


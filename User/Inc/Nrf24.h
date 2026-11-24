#ifndef __NRF24_H__
#define __NRF24_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * nRF24L01+ 上行发送（只发不收）。
 *
 * 硬件：SPI2（PB13=SCK / PB14=MISO / PB15=MOSI），CSN=PD9(WIFI_CS)，
 *       CE=PD11(WIFI_CE)。IRQ(PD10) 不用，靠读 STATUS 判状态。
 * ⚠️ SPI2 在 MX_SPI2_Init 的 USER CODE 里改成 8 分频 = 5.25 MHz，
 *    因为 nRF24 的 SPI 上限是 10 MHz。
 * ⚠️ 模块电源必须就近并 10 uF + 100 nF，否则电机换向时大量掉包。
 */

/* 单包上限，nRF24 固定 32 B */
#define NRF24_PAYLOAD_MAX   32U

/* 射频配置：信道 76、1 Mbps、0 dBm、2 字节 CRC、关自动应答 */
#define NRF24_RF_CHANNEL    76U

/* 上电初始化：配寄存器并进入 TX 模式，阻塞约 7 ms。
   返回 false 说明 SPI 不通或模块没供电（RF_CH 读回不对） */
bool nrf_init(void);

/* 发一包，len 超 32 会被截断。不阻塞：FIFO 满直接丢帧返回 false */
bool nrf_send(const uint8_t *data, uint8_t len);

/* 最近一次发送的状态字（bit5=TX_DS 成功，bit4=MAX_RT 失败），调试用 */
uint8_t nrf_last_status(void);

#endif

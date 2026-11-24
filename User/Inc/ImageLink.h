#ifndef __IMAGELINK_H__
#define __IMAGELINK_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * 无线图传链路（SPI2，PD9=CS / PD11=CE，与 nRF24 同一插槽）。
 *
 * ⚠️ 本文件目前只是接口契约，尚无实现。实现前不要从任何 .c 里 include。
 *    物理上与 nRF24 互斥：两者共用同一条片选与使能线。
 *
 * 与 Telemetry 的分工：
 *   Telemetry : 小帧、双向（上行遥测 + 下行控制），驱动见 Nrf24.c
 *   ImageLink : 大块单向（只发图像），不承载遥测帧
 *
 * 将来的实现要满足：
 *   1. 一次提交一整帧图像，内部按链路 MTU 自行分包
 *   2. 发送不阻塞调用方；忙时返回 false 丢帧，绝不等待
 *   3. 与 ESP32(WIFI_ESP_CS=PD8) 共处 SPI2 时，由本模块负责总线互斥
 *
 * 预期接口（实现时再在头文件里打开声明）：
 *   bool ImageLink_Init(void);
 *   bool ImageLink_Send(const uint8_t *data, uint32_t len);
 */

/* 链路单次提交上限，字节。留空值仅作占位，实现时按实际 MTU 定 */
#define IMAGELINK_BLOCK_MAX     0U

#endif

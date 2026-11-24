#ifndef __TELEMETRY_H__
#define __TELEMETRY_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * 无线遥测：把浮点快照打成小帧经 SPI2 射频链路发出，并可回收下行控制帧。
 * 采集在 Monitor，寄存器时序在 Nrf24；本模块只管帧格式与序号。
 */

/* 单包能装几路 float：32 B 去掉 seq(2) + crc(2) 只剩 28 B */
#define TELEMETRY_RADIO_FIELDS  7U

/* 下行链路：0 = 只发。开启前必须先让驱动层实现接收（Nrf24 现在只配了发送模式） */
#define TELEMETRY_HAVE_DOWNLINK 0

/* 上电调一次：初始化射频，返回 false 说明 SPI 不通或模块没供电 */
bool Telemetry_Init(void);

/* 发一帧，最多 TELEMETRY_RADIO_FIELDS 路；false = 上一帧还在发，本帧已丢 */
bool Telemetry_Send(const float *values, uint16_t count);

#if (TELEMETRY_HAVE_DOWNLINK)
/* 收一帧下行控制数据，返回字节数，0 = 无数据 */
uint16_t Telemetry_Recv(uint8_t *buf, uint16_t max);
#endif

#endif


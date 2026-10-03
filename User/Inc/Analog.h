#ifndef __ANALOG_H__
#define __ANALOG_H__

#include "main.h"
#include "adc.h"

/*
 * VBAT = Vpin × VOLTAGE_RATIO。分压比 10k/(39k+10k) = 16.17，
 * 15.68 是实测标定值；换板或换电阻需重标（分压线性，单点即可）。
 */
#define VOLTAGE_RATIO  15.68f

#define ADC_VREF        3.3f     /* VDDA，也是 ADC 参考电压 */
#define ADC_FULL_SCALE  4095.0f  /* 12 位分辨率 */

/*
 * 内部温度传感器：出厂校准值在系统存储器（STM32F4x9/429/439 除外）。
 * T = 30 + (raw - CAL1) × 80 / (CAL2 - CAL1)，CAL1/CAL2 = 30/110°C 时的 raw；
 * raw 需先按实际 Vref 归一化到 3.3V。传感器负温度系数 -> CAL2 < CAL1，自然抵消。
 */
#define TEMP_CAL1_ADDR    ((const uint16_t *)0x1FFF7A2CU)
#define TEMP_CAL2_ADDR    ((const uint16_t *)0x1FFF7A2EU)
#define TEMP_CAL1_DEGC    30.0f
#define TEMP_CAL2_DEGC    110.0f
#define TEMP_CAL_VREF     3.3f   /* 出厂校准时使用的 Vref+ */

/* 校准值不可用时的退路。F4 是 1.43V / 4.3mV，L4/F7/H7 才是 0.76V / 2.5mV，别搞混 */
#define TEMP_TYP_V25        1.43f
#define TEMP_TYP_AVG_SLOPE  0.0043f  /* V/°C */
#define TEMP_TYP_AMBIENT    25.0f

/* 一阶 IIR 低通系数。块率 500Hz（T8_TRGO 4kHz）→ τ≈73ms、fc≈2.2Hz */
#define ANALOG_LPF_ALPHA   0.027f

/* 启动 ADC + DMA 乒乓。会先钉死 DMA 相位再交还 TIM8 触发，这几步不能省。 */
void Analog_Init(void);
float Analog_ReadVoltage(void);
float Analog_ReadTemperature(void);

/*
 * 电压通道 ADC 码（0..4095），块内 8 点平均、不过 IIR，每 2 ms 更新（回调 500 Hz）。
 * 不要改成单点：单点噪声会在波形页上变成锯齿。
 */
uint16_t Analog_ReadVoltageCode(void);


#endif
#ifndef TFT_DRIVER_H
#define TFT_DRIVER_H

#include "TFT.h"
#include "TFT_Config.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * 本模块是当前工程的固定硬件适配边界：ST7735S 控制器、SPI1 和这块模组的
 * 全部参数都由 TFT_Config.h 提供。这里不引入总线抽象或控制器表。
 */

/** 初始化 ST7735S，清空可见区后点亮背光。 */
TFT_Status TFT_DriverInit(void);

/**
 * 丢弃驱动状态，屏幕已经断电或被拔掉时使用。
 *
 * 与 TFT_DriverInit() 不同，本函数不向 SPI 发任何命令、不等异步传输结束，
 * 只把本地状态和背光恢复成“没有屏幕”的样子，保证下一次 TFT_DriverInit()
 * 可以从确定状态重新开始。
 */
void TFT_DriverDeInit(void);

/** 阻塞发送核心准备好的全部差异页，返回时传输已经完成。 */
TFT_Status TFT_DriverWriteBlocking(void);

/** 使用 SPI 中断依次发送寻址命令和页数据。 */
TFT_Status TFT_DriverWriteIT(void);

/** 使用中断发送寻址命令、DMA 发送页数据。 */
TFT_Status TFT_DriverWriteDMA(void);

/** 查询驱动异步状态机是否正在传输。 */
bool TFT_DriverIsBusy(void);

/** ST7735S 没有对比度寄存器，固定返回 TFT_UNSUPPORTED。 */
TFT_Status TFT_DriverSetContrast(uint8_t value);

/** 发送 SLPIN 或 SLPOUT+DISPON，并同步背光。 */
TFT_Status TFT_DriverSetPowerSave(bool enable);

/** 直接开关背光；背光由应用控制，与总线状态无关。 */
TFT_Status TFT_DriverSetBacklight(bool enable);

/** 设置背光亮度，level 有效范围 0～TFT_BACKLIGHT_LEVEL_MAX。 */
TFT_Status TFT_DriverSetBacklightLevel(uint8_t level);

/* 由应用 HAL 回调转发，不直接占用 HAL 全局弱回调。 */
/** 处理一次 SPI 发送完成事件并推进异步状态机。 */
void TFT_DriverHandleTxComplete(void);

/** 终止当前异步传输，并向核心报告错误。 */
void TFT_DriverHandleError(void);

#endif /* TFT_DRIVER_H */

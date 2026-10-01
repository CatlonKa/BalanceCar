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
 * 关闭屏幕并丢弃驱动状态。
 *
 * 除了清本地状态、停背光，还会关显示并让面板进睡眠（DISPOFF + SLPIN），
 * 所以屏幕即使一直插着也会明确黑掉，不用拔线就能分清运动状态和 UI 状态。
 * 对已经断电的屏幕发命令同样安全，不会失败。
 *
 * 与 TFT_DriverInit() 不同，本函数不等异步传输结束；若调用时正在传输，
 * 则只关背光、跳过命令（避免把总线上的页数据穿插成乱码）。
 */
void TFT_DriverDeInit(void);

/** 阻塞发送核心准备好的全部差异页，返回时传输已经完成。 */
TFT_Status TFT_DriverWriteBlocking(void);

/** 使用 SPI 中断依次发送寻址命令和页数据。 */
TFT_Status TFT_DriverWriteIT(void);

/** 使用中断发送寻址命令、DMA 发送页数据。 */
TFT_Status TFT_DriverWriteDMA(void);

/**
 * 把一块 RGB565 图像直接写进面板，绕过 1bpp 帧缓冲。
 *
 * rgb565 是行优先、每像素两字节、**高字节在前**（与 ST7735S 线上顺序一致），
 * 所以数据可以从 Flash 直接送到 SPI，既不需要解包也不需要 RAM 中转。
 * 这与帧缓冲里的 1bpp 页式数据完全不同，是给开机图、摄像头这类
 * 「不由图形核心生成」的图像用的通路。
 *
 * (x, y) 以面板可见区左上角为原点，是**物理坐标，不受画布旋转影响**。
 * 矩形必须完整落在可见区内，越界返回 TFT_UNSUPPORTED。
 *
 * 全程阻塞。内部按行分块发送，以绕开单次传输长度是 uint16_t 的限制，
 * 所以整屏（128 x 160 x 2 = 40960 字节）也能一次写完。期间片选保持有效。
 */
TFT_Status TFT_DriverDirectBlit(int16_t x, int16_t y, uint16_t width, uint16_t height,
                                const uint8_t *rgb565);

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

#ifndef TFT_INTERNAL_H
#define TFT_INTERNAL_H

#include "TFT.h"
#include "TFT_Driver.h"

#include <stdbool.h>
#include <stdint.h>

/** 单个页式帧缓冲区的字节数。 */
#define TFT_BUFFER_SIZE (TFT_PHYSICAL_WIDTH * TFT_PHYSICAL_PAGES)

/* 图形文件只通过这些窄接口访问全局绘制状态。 */
/** 按当前前景模式绘制一个逻辑坐标像素。 */
void TFT_InternalPlot(int16_t x, int16_t y);

/** 按文字/位图的源像素值和背景模式写入一个逻辑坐标像素。 */
void TFT_InternalPlotSource(int16_t x, int16_t y, bool source_pixel);

/** 获取当前画布旋转后的逻辑宽度。 */
uint16_t TFT_InternalGetLogicalWidth(void);

/** 获取当前画布旋转后的逻辑高度。 */
uint16_t TFT_InternalGetLogicalHeight(void);

/** 读取当前半开区间裁剪窗口。 */
void TFT_InternalGetClip(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1);

/** 将半开包围盒收缩到当前裁剪窗口；没有交集时返回 false。 */
bool TFT_InternalIntersectClip(int32_t *x0, int32_t *y0,
                                int32_t *x1, int32_t *y1);

/** 查询整数角度对应的 Q15 正弦值。 */
int16_t TFT_InternalSinQ15(int16_t angle);

/** 查询整数角度对应的 Q15 余弦值。 */
int16_t TFT_InternalCosQ15(int16_t angle);

/* 异步驱动在中断回调中读取已冻结的传输快照。 */
/** 获取本次刷新期间保持不变的帧缓冲区。 */
const uint8_t *TFT_InternalGetTransferBuffer(void);

/** 获取指定页的首个差异列；无差异页返回物理宽度。 */
uint8_t TFT_InternalGetTransferMinX(uint8_t page);

/** 获取指定页的最后一个差异列。 */
uint8_t TFT_InternalGetTransferMaxX(uint8_t page);

/** 由驱动在异步传输结束时提交最终结果。 */
void TFT_InternalTransferFinished(TFT_Status status);

#endif

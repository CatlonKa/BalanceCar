#ifndef __KK_UI_PAGE_CUBE_H__
#define __KK_UI_PAGE_CUBE_H__

/*
 * 姿态立方体页：用线框立方体显示小车三维姿态，KEY1 归零水平朝向。
 *
 * 数据来自 UIAttitude（四元数，360° 全姿态），与控制的 Attitude 无关。
 * 页面只在可见时更新，退出即停，不占 CPU。
 */

#include <stdbool.h>
#include <stdint.h>

#include "KK_UI.h"

/* 页面进入：标定零偏 + 按当前重力重置姿态（约 0.2 s 阻塞）。 */
void KK_UI_PageCube_OnEnter(void);

/* 页面离开：停掉更新。 */
void KK_UI_PageCube_OnLeave(void);

/* 输入：确定键 = yaw 归零。 */
void KK_UI_PageCube_OnInput(KK_UI_InputEvent event);

/* 每轮调用：按固定周期积分并请求局部重绘。返回 true 表示需要重画。 */
bool KK_UI_PageCube_OnTick(uint32_t now_ms);

/* 绘制：只画立方体所在区域。 */
void KK_UI_PageCube_OnDraw(int16_t x_offset, int16_t clip_x, uint16_t clip_width);

#endif

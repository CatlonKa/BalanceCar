#ifndef __KK_UI_PAGE_CUBE_H__
#define __KK_UI_PAGE_CUBE_H__

/*
 * 姿态立方体页：线框立方体 + 车头炮管，显示小车三维姿态。
 *
 * 观察方向：镜头在**车尾**，朝车头看（人的朝向与车一致）。所以
 *   屏幕右 = 车右侧，屏幕上 = 车顶，左右倾斜按实物方向显示。
 * 方体沿机体 -Y（车头）伸出一根方形长方体当“炮管”，用来分辨朝向。
 *
 * 数据来自 UIAttitude（四元数，360° 全姿态），与控制的 Attitude 无关。
 * 页面只在可见时更新，退出即停，不占 CPU。
 */

#include <stdbool.h>
#include <stdint.h>

#include "KK_UI.h"

/* 页面进入：开始标定（非阻塞，标定期间显示动态提示）。 */
void KK_UI_PageCube_OnEnter(void);

/* 重新开始标定（KEY1）：重标零偏，并把当前水平朝向设为基准。 */
void KK_UI_PageCube_RestartCalib(void);

/* 页面离开：停掉更新。 */
void KK_UI_PageCube_OnLeave(void);

/* 输入：界面键不做事（归零/重标只绑 KEY1）。 */
void KK_UI_PageCube_OnInput(KK_UI_InputEvent event);

/* 每轮调用：按固定周期积分并请求局部重绘。返回 true 表示需要重画。 */
bool KK_UI_PageCube_OnTick(uint32_t now_ms);

/* 绘制：只画立方体所在区域。 */
void KK_UI_PageCube_OnDraw(int16_t x_offset, int16_t clip_x, uint16_t clip_width);

#endif

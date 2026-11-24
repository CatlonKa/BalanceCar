#ifndef __DEMO_H__
#define __DEMO_H__

/*
 * 阶段性 Demo：8 字轨迹演练（直走 → 右弧 → 直走 → 左弧）。
 *
 * ⚠️ 本模块不属于最终产品架构，最终巡线/竞速成品不需要它。
 *    边界、剧本、验收与移除方法见同目录 README.md。
 *
 * 唯一输出口是 Control_SetTarget()，绝不直接调 motor_drive()。
 */

#include <stdbool.h>

/* 总开关：0 = 完全不参与编译（调用点用 #if 包住），默认 0 */
#define DEMO_ENABLE     0

/* 上电调一次，清剧本索引与计时 */
void Demo_Init(void);

/* TIM7 1 kHz 调用，**必须在 Control_Update() 之前**：先下目标，再算输出 */
void Demo_Update(void);

/* true = 剧本正在推进；仅供遥测/调试 */
bool Demo_IsActive(void);

#endif

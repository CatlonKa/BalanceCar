#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <stdbool.h>

/*
 * 显示层生命周期。
 *
 * 屏幕在电机转动时会被拔掉/断电，所以它的初始化和释放不能按「上电一次」来做，
 * 而是每次车轮归属发生变化时重新走一遍。这一层负责把车轮归属（car_run）
 * 与屏幕的供电状态对齐，让 main.c 的主循环只需要问一句「现在能不能碰 UI」。
 *
 * 与 TFT 目录里其它文件不同，本文件是**应用胶水层**：
 *   TFT.h / TFT_Config.h / TFT_Driver.h / TFT_Internal.h 都不依赖上层，
 *   而本文件要同时用到 TFT 公共接口和 KK_UI 应用层，所以它是 TFT 目录里
 *   唯一反向依赖 UI 的文件。放在这里只是因为「和屏幕显示有关的代码都在 TFT 目录」，
 *   不要照着它的样子给驱动层加向上依赖。
 *
 * 依赖方向：event（car_run）-> TFT_Display -> TFT + KK_UI 应用层。
 */

/**
 * 上电调用一次：此时屏幕还没供电，把显示层置成未就绪状态，并确保背光不亮。
 * 不向总线发任何命令，所以屏幕没接也安全。
 */
void TFT_DisplayInit(void);

/**
 * 按车轮归属同步显示层，返回同步后屏幕是否可用。主循环每轮调用一次。
 *
 * 返回 false 时调用方必须完全不碰 UI：不取输入、不绘制、不提交刷新、不改背光。
 * 车轮归属没变时不做任何事，直接返回当前状态。
 */
bool TFT_DisplaySync(void);

#endif /* TFT_DISPLAY_H */

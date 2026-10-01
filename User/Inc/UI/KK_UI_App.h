#ifndef KK_UI_APP_H
#define KK_UI_APP_H

#include "KK_UI.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KK_UI 的应用层。
 *
 * 这一层持有页面拓扑、字体、图标、业务变量和业务事件；KK_UI 核心负责导航、
 * 交互、动画、整屏绘制和刷新调度。业务代码只读写下面的 KK_UI_AppState，
 * 不直接清屏，也不自己调用 TFT_Update*()。
 */

/** 应用层业务事件 ID，取值必须非零。 */
typedef enum {
    KK_UI_APP_EVENT_BUZZER_CHANGED = 1, /**< 蜂鸣器开关被修改。 */
    KK_UI_APP_EVENT_LIMIT_CHANGED,      /**< 限速被修改。 */
    KK_UI_APP_EVENT_BACKLIGHT_CHANGED,  /**< 背光亮度被修改。 */
    KK_UI_APP_EVENT_RESET_CONFIRMED     /**< 用户确认了重置。 */
} KK_UI_AppEvent;

/**
 * 应用展示状态。
 *
 * 业务模块直接写这些字段；写入字符缓冲区后如果需要立刻刷新界面，
 * 调用 KK_UI_Invalidate()。字符缓冲区必须保持 NUL 结尾。
 */
typedef struct {
    bool buzzer_on;            /**< 蜂鸣器开关，绑定到菜单项。 */
    int32_t speed_limit;       /**< 限速百分比，绑定到菜单项。 */
    int32_t backlight_level;   /**< 背光亮度百分比（0～100），绑定到菜单项。 */
    char voltage_text[12];     /**< 状态页“电压”一行的值。 */
    char angle_text[12];       /**< 状态页“角度”一行的值。 */
    char speed_text[12];       /**< 状态页“速度”一行的值。 */
} KK_UI_AppState;

extern KK_UI_AppState kk_ui_app_state;

/**
 * 建立静态页面拓扑并初始化 KK_UI。
 * 调用前屏幕必须已经由 TFT_Init() 初始化成功。
 */
KK_UI_Status KK_UI_AppInit(void);

/**
 * 取一次输入、推进 KK_UI，并处理已经排队的业务事件。
 * 主循环中调用间隔不得大于 10 ms；绘制与提交由 KK_UI 自己按帧间隔节流。
 */
void KK_UI_AppUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* KK_UI_APP_H */

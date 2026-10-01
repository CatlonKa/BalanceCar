#ifndef KK_UI_INPUT_H
#define KK_UI_INPUT_H

#include "KK_UI.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 把本工程的实际输入硬件归一化成 KK_UI_Input。
 *
 * 本工程没有独立的 UI 按键或旋钮：可用的操作手段是左右两个车轮编码器
 * （左轮接 TIM1 的 ENCODER_AL/BL，右轮接 TIM4 的 ENCODER_AR/BR），
 * 所以整套 UI 只能在电机不转、车轮可以用手拨动时使用。
 *
 * 两个阶段（由 event.c 的 car_run 区分，编码器同一时刻只归一方）：
 *   car_run = 1（电机在转）→ 车轮归电机，TIM6 做测速，UI 冻结并关背光节能；
 *   car_run = 0（已停机）  → 车轮归 UI，TIM6 关掉，静默一段后开始接受手势。
 *
 * 归一化后的语义：
 *   左轮前进（计数增加） -> 确定
 *   左轮后退（计数减少） -> 取消 / 返回
 *   右轮前进（计数增加） -> 上一个
 *   右轮后退（计数减少） -> 下一个
 * 每个方向的累计计数都要达到一定步数才认定为一次按键，防止轻微抖动误触发；
 * 两次手势之间还要间隔 KK_UI_INPUT_GESTURE_INTERVAL_MS（见 KK_UI.h），
 * 间隔之内的转动会被读掉丢弃，避免手轮惯性把一次拨动算成好几格。
 */

/*
 * 车轮数据源开关。
 *
 * 1（默认）—— 接真实车轮：读 encoder_get_delta_left/right()，
 *             并用 event.h 的 car_run 判断车轮当前归谁。
 * 0 —— 退路：手势冻结、界面照常显示，方便单独上屏验证布局与刷新。
 *             这时不依赖 encoder / event 模块。
 */
#ifndef KK_UI_INPUT_HAS_WHEEL
#define KK_UI_INPUT_HAS_WHEEL 1
#endif

/*
 * 手势判定阈值 KK_UI_INPUT_WHEEL_STEPS 定义在 KK_UI.h 的「本工程适配参数」一节，
 * 那里集中放了可以调的宏，方便一处修改。
 */

/**
 * 读取一次归一化输入。
 * now_ms 由调用方提供，与传给 KK_UI_Update() 的时间基准必须一致。
 */
KK_UI_Input KK_UI_InputRead(uint32_t now_ms);

/**
 * 当前车轮是否可以作为 UI 输入（也就是“电机已停止”）。
 *
 * 界面用它决定要不要推进 UI、要不要点亮背光。
 */
bool KK_UI_InputWheelUsable(void);

/**
 * 复位手势状态机。在阶段切换时调用：电机开始输出推力前清一次、
 * 主动停机后再清一次。
 *
 * 只清 UI 自己的软件累计量：不碰 TIM1 / TIM4 的硬件计数器，
 * 也不碰 User/Src/event.c 里 TIM6 那套测速基准。
 */
void KK_UI_InputReset(void);

/**
 * 取走“是否发生过一次返回手势”的标记。
 *
 * 左轮后退就是取消/返回，应用层拿到这个标记后统一交给 KK_UI_RequestBack()：
 * 有弹框就关掉弹框（编辑器丢弃草稿、确认框按取消、提示框按知道了），
 * 否则退回上一页。
 */
bool KK_UI_InputTakeBackRequest(void);

#ifdef __cplusplus
}
#endif

#endif /* KK_UI_INPUT_H */

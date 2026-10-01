#ifndef KK_UI_INTERNAL_H
#define KK_UI_INTERNAL_H

#include "KK_UI.h"
#include "KK_UI_Draw.h"
#include "TFT.h"

#include <stddef.h>

#define KK_UI_Q12_ONE 4096U

#define KK_UI_HOME_MS             300U
#define KK_UI_MENU_MS             200U
#define KK_UI_PAGE_MS             220U
#define KK_UI_INFO_SCROLL_MS      180U
#define KK_UI_DIALOG_OPEN_MS      180U
#define KK_UI_DIALOG_CLOSE_MS     150U
#define KK_UI_FOCUS_MS            160U
#define KK_UI_INT_VALUE_MS        100U
#define KK_UI_BOOL_VALUE_MS       120U
#define KK_UI_TOAST_MS            120U

/*
 * 逻辑画布与纵向布局常量。
 *
 * 上游参考布局是 128 x 64 横屏；本工程面板物理为 128 x 160 竖屏，
 * 宽度与参考一致，因此所有 128 宽相关的横向坐标保持不变，
 * 这里只把纵向常量按实际高度重新确定，并集中放在一处便于复核。
 *
 * 纵向预算（128 x 160）：
 *   菜单/信息页 = 标题 18 + 行高 20 x 7 行 = 158，剩余 2 像素。
 */
#define KK_UI_SCREEN_WIDTH        128
#define KK_UI_SCREEN_HEIGHT       160
#define KK_UI_MENU_HEADER_HEIGHT  18
#define KK_UI_INFO_HEADER_HEIGHT  18
#define KK_UI_ROW_HEIGHT          20
#define KK_UI_VISIBLE_ROWS        7
#define KK_UI_PAGE_PARALLAX       32

/*
 * 首页竖向轮播的几何。
 *
 * 间距同时决定两件事：选中项与相邻项的距离、以及最外两项露出多少。
 * 以选中项居中、选中放大到 KK_UI_HOME_ICON_FOCUS 为前提，纵坐标是
 *
 *     中心 y = CENTER_Y + 偏移 * ITEM_SPACING
 *     方框   = 中心 y ± 尺寸/2
 *
 * 当前取值下的实际排布（CENTER_Y=80，普通 32、选中 64）：
 *
 *   偏移  中心 y   图标方框        可见情况
 *     -2      -8    -24 ..   8     只露下缘 8px
 *     -1      36     20 ..  52     完整 32
 *      0      80     48 .. 112     完整 64（选中）
 *     +1     124    108 .. 140     完整 32
 *     +2     168    152 .. 184     只露上缘 8px
 *
 * 于是选中项与上下相邻项各重叠 4px。想让重叠更少就调大间距，但最外两项
 * 露出的部分会同步变少；间距到 48 时重叠归零、最外两项也完全滑出屏幕。
 * 反过来调小间距会让重叠变多。
 *
 * 放大项的尺寸和横向位置都随「离选中位的距离」连续变化，所以换项时旧项会
 * 一边缩小一边向左退回、新项一边放大一边向右靠过来，不需要另一套动画状态：
 *   选中时：64x64，中心 x = 46（向右靠）
 *   离一个间距以上：32x32，中心 x = 30
 * 标签也跟着走：选中时右对齐到屏幕右边，未选中时靠左固定在 TEXT_LEFT。
 */
#define KK_UI_HOME_ITEM_SPACING   44  /**< 相邻两项中心 y 的间距，同时决定边缘露出量与重叠量。 */
#define KK_UI_HOME_CENTER_Y       80  /**< 选中项中心 y（画布纵向正中）。 */
#define KK_UI_HOME_ICON_SIZE      32  /**< 图标资源的边长（XBM 源尺寸）。 */
#define KK_UI_HOME_ICON_FOCUS     64  /**< 选中项放大后的边长，取了两倍。 */
#define KK_UI_HOME_ICON_CENTER_X  30  /**< 未选中项的图标中心 x。 */
#define KK_UI_HOME_ICON_FOCUS_X   46  /**< 选中项的图标中心 x，向右靠。 */
#define KK_UI_HOME_TEXT_LEFT      62  /**< 未选中项的标签左边缘 x。 */
#define KK_UI_HOME_TEXT_RIGHT_GAP 4   /**< 选中项的标签离屏幕右边的留白。 */
#define KK_UI_HOME_LABEL_HEIGHT   18  /**< 标签行高，反白底的方框高度。 */

/*
 * 弹框纵向基准。
 *
 * 弹框内部坐标在参考布局中都以“打开后盒顶 y = 1”为基准，
 * 本工程把打开后的盒顶整体下移到 KK_UI_DIALOG_TOP，使 61 像素高的弹框
 * 在 160 像素高的画布上居中（(160 - 61) / 2 = 49）。
 * kk_ui_dialog.c 中所有弹框内部纵向坐标都已经按这个基准重算。
 */
#define KK_UI_DIALOG_TOP          49

typedef struct {
    KK_UI_PageId page;
    uint16_t selected;
    uint16_t top;
    int32_t scroll_q8;
    int32_t focus_q8;
} KK_UI_PageState;

typedef struct {
    uint32_t changed_at;
    uint32_t pressed_at;
    uint32_t repeat_at;
    uint8_t candidate;
    uint8_t stable;
    uint8_t armed;
} KK_UI_KeyState;

typedef struct {
    int32_t scroll_from_q8;
    int32_t focus_from_q8;
    uint32_t started;
    uint16_t duration;
    uint8_t active;
} KK_UI_ListAnimation;

typedef struct {
    int32_t scroll_from_q8; /**< 换项动画的起始滚动位置，单位 Q8 像素。 */
    uint32_t started;       /**< 动画起点时刻。 */
    uint8_t active;         /**< 是否正在播放换项动画。 */
} KK_UI_HomeAnimation;

typedef struct {
    KK_UI_PageState outgoing;
    uint32_t started;
    uint8_t active;
    uint8_t reverse;
    uint8_t leave_custom;
} KK_UI_PageTransition;

typedef enum {
    KK_UI_OVERLAY_NONE = 0,
    KK_UI_OVERLAY_INT,
    KK_UI_OVERLAY_BOOL,
    KK_UI_OVERLAY_CONFIRM,
    KK_UI_OVERLAY_MESSAGE
} KK_UI_OverlayType;

/*
 * 弹框焦点。取消已改成左轮后退手势、不再是可选项，所以确认框只有 CONFIRM，
 * 整数/开关编辑器只有 VALUE 与 CONFIRM。
 */
typedef enum {
    KK_UI_FOCUS_VALUE = 0,
    KK_UI_FOCUS_CONFIRM
} KK_UI_DialogFocus;

typedef struct {
    KK_UI_OverlayType type;
    uint16_t ref;
    const char *text;
    KK_UI_EventId event;
    int32_t draft;
    int32_t original;
    int32_t previous;
    int32_t focus_x_q8;
    int32_t focus_y_q8;
    int32_t focus_w_q8;
    int32_t focus_h_q8;
    int32_t focus_from_x_q8;
    int32_t focus_from_y_q8;
    int32_t focus_from_w_q8;
    int32_t focus_from_h_q8;
    uint32_t phase_started;
    uint32_t focus_started;
    uint32_t value_started;
    int16_t origin_y;
    uint16_t phase_q12;
    uint16_t phase_from_q12;
    uint16_t value_q12;
    uint8_t focus;
    int8_t value_direction;
    uint8_t editing;
    uint8_t closing;
    uint8_t focus_moving;
    uint8_t value_moving;
} KK_UI_Overlay;

typedef struct {
    const char *text;
    uint32_t shown_at;
    uint32_t expires_at;
    uint16_t phase_q12;
    uint8_t visible;
    uint8_t closing;
} KK_UI_Toast;

typedef enum {
    KK_UI_DEFER_NONE = 0,
    KK_UI_DEFER_CLOSE,
    KK_UI_DEFER_FINISH,
    KK_UI_DEFER_BACK,
    KK_UI_DEFER_INT,
    KK_UI_DEFER_BOOL,
    KK_UI_DEFER_CONFIRM,
    KK_UI_DEFER_MESSAGE,
    KK_UI_DEFER_TOAST
} KK_UI_DeferredType;

typedef struct {
    KK_UI_DeferredType type;
    uint16_t ref;
    KK_UI_EventId event;
    const char *text;
    uint32_t duration;
} KK_UI_Deferred;

typedef struct {
    const KK_UI_App *app;
    KK_UI_PageState current;
    KK_UI_PageState stack[KK_UI_NAV_DEPTH];
    KK_UI_PageTransition transition;
    KK_UI_HomeAnimation home_anim;
    KK_UI_ListAnimation list_anim;
    KK_UI_Overlay overlay;
    KK_UI_Toast toast;
    KK_UI_Deferred deferred;
    KK_UI_KeyState keys[3];
    KK_UI_EventId events[KK_UI_EVENT_QUEUE_LENGTH];
    KK_UI_ErrorInfo error;
    uint32_t last_update;
    uint32_t next_frame;
    uint32_t transfer_started;
    int16_t dirty_x;      /* 本帧脏矩形左边界，包含。 */
    int16_t dirty_y;      /* 本帧脏矩形上边界，包含。 */
    int16_t dirty_x1;     /* 本帧脏矩形右边界，不包含。 */
    int16_t dirty_y1;     /* 本帧脏矩形下边界，不包含。 */
    uint8_t stack_count;
    uint8_t event_head;
    uint8_t event_count;
    uint8_t initialized;
    uint8_t keys_initialized;
    uint8_t transfer_active;
    uint8_t frame_ready;
    uint8_t dirty;
    uint8_t dirty_full;   /* 本帧必须整屏重绘，不允许局部重绘。 */
    uint8_t display_fault;
    uint8_t error_valid;
    uint8_t in_callback;
    uint8_t reject_flash;
    uint8_t blocked_keys;
} KK_UI_Runtime;

extern KK_UI_Runtime kk_ui;

uint16_t KK_UI_EaseQ12(uint32_t elapsed, uint32_t duration);
int32_t KK_UI_LerpQ12(int32_t from, int32_t to, uint16_t progress);
int16_t KK_UI_RoundQ8(int32_t value);

void KK_UI_RecordError(KK_UI_Status code, KK_UI_PageId page, uint16_t index);
bool KK_UI_QueueEvent(KK_UI_EventId event);
const KK_UI_PageRoute *KK_UI_GetRoute(KK_UI_PageId page);
KK_UI_PageType KK_UI_CurrentPageType(void);

void KK_UI_ResetPageState(KK_UI_PageState *state, KK_UI_PageId page);
KK_UI_Status KK_UI_NavigateTo(KK_UI_PageId page, uint32_t now);
KK_UI_Status KK_UI_NavigateBack(uint32_t now);
void KK_UI_AnimateNavigation(uint32_t now);
void KK_UI_DrawScene(uint32_t now);
void KK_UI_DrawPage(const KK_UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width, uint32_t now);

void KK_UI_HomeAnimate(uint32_t now);
void KK_UI_HomeInput(KK_UI_InputEvent event, uint32_t now);
void KK_UI_HomeDraw(const KK_UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width);

KK_UI_ItemState KK_UI_MenuGetItemState(const KK_UI_MenuPage *page,
                                       uint16_t item_index);
void KK_UI_MenuRepairFocus(KK_UI_PageState *state);
void KK_UI_MenuAnimate(uint32_t now);
void KK_UI_MenuInput(KK_UI_InputEvent event, uint32_t now);
void KK_UI_MenuDraw(const KK_UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width);

void KK_UI_InfoAnimate(uint32_t now);
void KK_UI_InfoInput(KK_UI_InputEvent event, uint32_t now);
void KK_UI_InfoDraw(const KK_UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width);
void KK_UI_CustomInput(KK_UI_InputEvent event);
void KK_UI_CustomTick(uint32_t now);

KK_UI_Status KK_UI_DialogOpenInt(uint16_t index, uint32_t now);
KK_UI_Status KK_UI_DialogOpenBool(uint16_t index, uint32_t now);
KK_UI_Status KK_UI_DialogOpenConfirm(uint16_t index, uint32_t now);
KK_UI_Status KK_UI_DialogOpenMessage(const char *text, KK_UI_EventId event,
                                     uint32_t now);
KK_UI_Status KK_UI_ToastOpen(const char *text, uint32_t duration,
                             uint32_t now);
void KK_UI_DialogInput(KK_UI_InputEvent event, uint32_t now);
void KK_UI_DialogAnimate(uint32_t now);
void KK_UI_DialogDraw(void);
void KK_UI_ToastDraw(void);
bool KK_UI_DialogActive(void);
void KK_UI_DialogDismiss(uint32_t now);

void KK_UI_DispatchInput(KK_UI_InputEvent event, uint32_t now);
void KK_UI_ApplyDeferred(uint32_t now);
KK_UI_Status KK_UI_Defer(KK_UI_DeferredType type, uint16_t ref,
                         const char *text, KK_UI_EventId event,
                         uint32_t duration);

void KK_UI_FormatInt(int32_t value, const char *unit, char *buffer,
                     size_t capacity);
void KK_UI_DrawCentered(int16_t x, int16_t y, uint16_t width,
                        const char *text);

#endif

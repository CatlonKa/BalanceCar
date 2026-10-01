#ifndef KK_UI_H
#define KK_UI_H

#include "KK_UI_Config.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t KK_UI_PageId;
typedef uint16_t KK_UI_EventId;

#define KK_UI_PAGE_NONE  ((KK_UI_PageId)0U)
#define KK_UI_EVENT_NONE ((KK_UI_EventId)0U)

/* ------------------------------------------------------- 本工程适配参数 ---- */
/*
 * 上游核心之外的工程专属参数集中放在这节，只影响手势判定和节流节奏，
 * 不改变核心状态机。容量类参数（导航深度、事件队列等）在 KK_UI_Config.h。
 */

/* 手势阈值：每个方向累计多少个编码器计数才算一次「上 / 下 / 确定」。 */
/* 调大 = 更迟钝、不易误触发；调小 = 更灵敏、轻轻一拨就动。 */
/* 编码器是 TIM_ENCODERMODE_TI1（只数 TI1 边沿、无四倍频），所以这就是线脉冲个数。 */
#define KK_UI_INPUT_WHEEL_STEPS 5000

/* 两次手势之间的最小冷却时间，毫秒。 */
/* 调大 = 一次拨动只算一格、但连拨变慢；调小 = 连拨更快、惯性可能多带一格。 */
/* 冷却期内的计数照读但立即丢弃，所以“拨一下后轮子惯性继续转”不会凑出第二格。 */
/* 必须大于 KK_UI_Input.c 里确定键的合成保持时间 KK_UI_INPUT_OK_HOLD_MS（40 ms）。 */
#define KK_UI_INPUT_GESTURE_INTERVAL_MS 300

/*
 * 漏积分：振动抑制与转速门槛。
 *
 * 累计量每隔 KK_UI_INPUT_DECAY_PERIOD_MS 毫秒，按自身大小的 1/2^SHIFT 往零削一次。
 * 它同时解决两件事：
 *
 *   1. 抖动抵消（用户报告的问题）
 *      车身振动、手抖带来的零散计数以前只会一直攒着，攒够阈值就误触发一次。
 *      加上漏积分后，这些少量计数会自己慢慢漏掉，攒不起来。
 *
 *   2. 转速门槛（“要有一定的转动速度才会切换”）
 *      只有输入的速率超过漏掉的速率，累计量才可能涨到阈值；持续慢速转动会停在
 *      一个低于阈值的平衡点上，永远不触发。这就是“必须转得够快”的实现方式，
 *      不需要另外再做一个瞬时速率判断。
 *
 * 平衡点：设输入速率 R（计数/秒），稳定时累计量约为
 *
 *     R * 2^SHIFT * KK_UI_INPUT_DECAY_PERIOD_MS / 1000
 *
 * 令它等于 KK_UI_INPUT_WHEEL_STEPS，就得到能触发所需的最小速率：
 *
 *     最小速率 ≈ WHEEL_STEPS * 1000 / (2^SHIFT * PERIOD)
 *
 * 按当前的 WHEEL_STEPS=5500、SHIFT=3、PERIOD=100 ms，约 6875 计数/秒，
 * 换个说法就是：5500 个计数要在 0.8 秒以内转完才有效。
 * 比这更慢的转动（慢拨、振动、手抖）累计量会停在阈值以下，不会触发。
 *
 * 注意这里用的是**指数**衰减而不是固定速率：它按比例削，与 WHEEL_STEPS 的
 * 绝对大小无关，所以以后调阈值不需要回来重调这两个参数。
 *   觉得还是太灵敏、容易误触发 -> 增大 SHIFT（削得更狠，最小速率变高）
 *   觉得要拨得很快才动、太费力 -> 减小 SHIFT 或增大 PERIOD
 */
#define KK_UI_INPUT_DECAY_PERIOD_MS 80U
#define KK_UI_INPUT_DECAY_SHIFT 2U

/* --------------------------------------------------------- 核心公共类型 ---- */

typedef enum {
    KK_UI_OK = 0,
    KK_UI_INVALID_ARGUMENT,
    KK_UI_NOT_INITIALIZED,
    KK_UI_CONFIG_ERROR,
    KK_UI_BUSY,
    KK_UI_QUEUE_FULL,
    KK_UI_NAVIGATION_FULL,
    KK_UI_UNSUPPORTED,
    KK_UI_DISPLAY_ERROR,
    KK_UI_DISPLAY_TIMEOUT,
    KK_UI_NOT_ALLOWED
} KK_UI_Status;

typedef struct {
    KK_UI_Status code;
    KK_UI_PageId page;
    uint16_t index;
} KK_UI_ErrorInfo;

enum {
    KK_UI_KEY_UP = 1U << 0,
    KK_UI_KEY_DOWN = 1U << 1,
    KK_UI_KEY_OK = 1U << 2
};

typedef struct {
    uint8_t keys;
    int16_t encoder_delta; /* Positive is Down; negative is Up. */
} KK_UI_Input;

typedef enum {
    KK_UI_INPUT_UP = 0,
    KK_UI_INPUT_DOWN,
    KK_UI_INPUT_OK
} KK_UI_InputAction;

typedef enum {
    KK_UI_INPUT_PRESS = 0,
    KK_UI_INPUT_REPEAT,
    KK_UI_INPUT_ENCODER
} KK_UI_InputSource;

typedef struct {
    KK_UI_InputAction action;
    KK_UI_InputSource source;
    uint16_t steps;
} KK_UI_InputEvent;

typedef enum {
    KK_UI_PAGE_HOME = 0,
    KK_UI_PAGE_MENU,
    KK_UI_PAGE_INFO,
    KK_UI_PAGE_CUSTOM
} KK_UI_PageType;

typedef struct {
    KK_UI_PageType type;
    uint16_t index;
} KK_UI_PageRoute;

typedef struct {
    const char *label;
    const uint8_t *icon_xbm_32x32;
    KK_UI_PageId target_page;
} KK_UI_HomeItem;

typedef struct {
    const KK_UI_HomeItem *items;
    uint16_t item_count;
} KK_UI_HomePage;

typedef enum {
    KK_UI_MENU_PAGE = 0,
    KK_UI_MENU_ACTION,
    KK_UI_MENU_INT,
    KK_UI_MENU_BOOL,
    KK_UI_MENU_CONFIRM
} KK_UI_MenuItemType;

typedef struct {
    const char *label;
    KK_UI_MenuItemType type;
    uint16_t ref;
} KK_UI_MenuItem;

typedef struct {
    const char *title;
    const KK_UI_MenuItem *items;
    uint8_t *item_states; /* Optional packed 2-bit state table. */
    uint16_t item_count;
} KK_UI_MenuPage;

typedef enum {
    KK_UI_ITEM_NORMAL = 0,
    KK_UI_ITEM_HIDDEN = 1,
    KK_UI_ITEM_DISABLED = 2,
    KK_UI_ITEM_RESERVED = 3
} KK_UI_ItemState;

typedef struct {
    const char *name;
    const char *value;
} KK_UI_InfoRow;

typedef struct {
    const char *title; /* NULL or empty omits the title bar. */
    const KK_UI_InfoRow *rows;
    uint16_t row_count;
} KK_UI_InfoPage;

typedef struct {
    const char *title;
    int32_t *value;
    int32_t minimum;
    int32_t maximum;
    uint32_t step;
    const char *unit;
    KK_UI_EventId changed_event;
} KK_UI_IntBinding;

typedef struct {
    const char *title;
    bool *value;
    KK_UI_EventId changed_event;
} KK_UI_BoolBinding;

typedef struct {
    const char *text;
    KK_UI_EventId confirmed_event;
    KK_UI_EventId cancelled_event;
} KK_UI_ConfirmDesc;

typedef struct {
    const uint8_t *home_font;
    const uint8_t *title_font;
    const uint8_t *body_font;
} KK_UI_Fonts;

/*
 * 固定文字。
 *
 * return_text / cancel_text 在本工程里是“保留但不绘制”的字段：菜单自动追加的
 * 「返回」项和弹框里的「取消」按钮都已移除，取消/返回统一由左轮后退手势完成，
 * 但核心初始化仍然要求这两个字段非空（传空字符串可以通过，传 NULL 会校验失败）。
 */
typedef struct {
    const char *return_text;
    const char *cancel_text;
    const char *confirm_text;
    const char *on_text;
    const char *off_text;
    const char *message_title;
} KK_UI_Texts;

typedef struct {
    KK_UI_PageId root_page;
    const KK_UI_PageRoute *routes;
    uint16_t route_count;
    const KK_UI_HomePage *home_pages;
    uint16_t home_page_count;
    const KK_UI_MenuPage *menu_pages;
    uint16_t menu_page_count;
    const KK_UI_InfoPage *info_pages;
    uint16_t info_page_count;
    uint16_t custom_page_count;
    const KK_UI_IntBinding *int_bindings;
    uint16_t int_binding_count;
    const KK_UI_BoolBinding *bool_bindings;
    uint16_t bool_binding_count;
    const KK_UI_ConfirmDesc *confirm_descs;
    uint16_t confirm_desc_count;
    KK_UI_Fonts fonts;
    KK_UI_Texts texts;
} KK_UI_App;

KK_UI_Status KK_UI_Init(const KK_UI_App *app);
KK_UI_Status KK_UI_Update(uint32_t now_ms, KK_UI_Input input);
void KK_UI_Invalidate(void);

/**
 * 声明当前画面的一个小矩形发生了变化，请求核心只重绘这一块。
 *
 * KK_UI_Invalidate() 要求整屏重画；本函数允许核心保留上一帧、只在该矩形内
 * 清除并重画，因此省掉整屏绘制与整屏传输两笔开销。
 *
 * 目前只有自定义页（KK_UI_PAGE_CUSTOM）会走局部重绘：核心会在脏矩形内调用
 * KK_UI_CustomOnDraw()，页面用 KK_UI_GetDirtyRect() 取回同一矩形来裁剪绘制。
 * 其它页面类型，以及出现切页动画、弹框、提示框或拒绝闪烁时，本函数退化为
 * 整屏重绘（画面依旧正确，只是没有局部重绘的收益）。
 *
 * 多次调用取并集；声明只对下一次绘制有效。页面必须保证「声明之外没有改动」，
 * 否则未声明的改动不会上屏。拿不准时改用 KK_UI_Invalidate()。
 */
void KK_UI_InvalidateRegion(int16_t x, int16_t y, uint16_t width, uint16_t height);

/**
 * 取回本次绘制需要重绘的矩形（半开区间）。
 *
 * 返回 true 表示核心正在做局部重绘，矩形已写入输出参数；返回 false 表示本帧
 * 会整屏重绘（自适应页应按整屏绘制）。任意输出参数都可以传 NULL。
 */
bool KK_UI_GetDirtyRect(int16_t *x, int16_t *y, uint16_t *width, uint16_t *height);

KK_UI_Status KK_UI_RecoverDisplay(void);

bool KK_UI_PollEvent(KK_UI_EventId *out_event);
bool KK_UI_PollError(KK_UI_ErrorInfo *out_error);
KK_UI_Status KK_UI_SetMenuItemState(KK_UI_PageId page,
                                    uint16_t item_index,
                                    KK_UI_ItemState state);

KK_UI_Status KK_UI_OpenIntEditor(uint16_t binding_index);
KK_UI_Status KK_UI_OpenBoolEditor(uint16_t binding_index);
KK_UI_Status KK_UI_OpenConfirm(uint16_t confirm_index);
KK_UI_Status KK_UI_ShowMessage(const char *text, KK_UI_EventId acknowledged_event);
KK_UI_Status KK_UI_ShowToast(const char *text, uint32_t duration_ms);

/**
 * 主动后退（取消）一步。
 *
 * 本工程的左轮后退手势统一落到这里：
 *   有弹框打开 -> 关掉弹框。编辑器丢弃草稿且不提交；确认框按「取消」处理
 *                （投递 cancelled_event）；提示框按「知道了」处理；
 *   普通页面   -> 退回上一页；
 *   自定义页   -> 与 KK_UI_CustomRequestClose() 等价。
 *
 * 与 KK_UI_CustomRequestClose() 的唯一区别是不限页面类型。已在根页面
 * （导航栈为空）时返回 KK_UI_NOT_ALLOWED，页面切换动画期间返回 KK_UI_BUSY。
 */
KK_UI_Status KK_UI_RequestBack(void);

KK_UI_Status KK_UI_CustomRequestClose(void);
KK_UI_Status KK_UI_CustomFinish(KK_UI_EventId event);

#if KK_UI_ENABLE_CUSTOM
/* Fixed application adapter. The application defines these five symbols. */
void KK_UI_CustomOnEnter(KK_UI_PageId page);
void KK_UI_CustomOnLeave(KK_UI_PageId page);
void KK_UI_CustomOnInput(KK_UI_PageId page, KK_UI_InputEvent event);
bool KK_UI_CustomOnTick(KK_UI_PageId page, uint32_t now_ms);
void KK_UI_CustomOnDraw(KK_UI_PageId page, int16_t x_offset,
                        int16_t clip_x, uint16_t clip_width);
#endif

#ifdef __cplusplus
}
#endif

#endif

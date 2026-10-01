#include "KK_UI_App.h"

#include "KK_UI_AppIcons.h"
#include "KK_UI_Input.h"
#include "KK_UI_Draw.h"
#include "KK_UI_FontZh16.h"

#include "TFT.h"
#include "main.h"

/*
 * KK_UI 最小可运行样例。
 *
 * 这一层只描述页面、绑定和业务事件，并把刷新所有权完全交给 KK_UI：
 * 这里不调用 TFT_Clear()，也不调用任何 TFT_Update*()。
 *
 * 页面拓扑（共 4 页）：
 *   1 首页（HOME）  -> 三个图标入口
 *   2 菜单（MENU）  -> 蜂鸣器开关 / 限速整数 / 重置确认
 *   3 状态（INFO）  -> 只读信息行
 *   4 波形（CUSTOM）-> 全屏自定义页
 */

/* --------------------------------------------------------------- 页面编号 ---- */

enum {
    KK_UI_APP_PAGE_HOME = 1, /**< 首页，同时也是根页面。 */
    KK_UI_APP_PAGE_MENU,     /**< 普通菜单页。 */
    KK_UI_APP_PAGE_STATUS,   /**< 只读信息页。 */
    KK_UI_APP_PAGE_WAVE      /**< 波形全屏自定义页。 */
};

/* --------------------------------------------------------------- 展示状态 ---- */

KK_UI_AppState kk_ui_app_state = {
    .buzzer_on = true,
    .speed_limit = 60,
    .backlight_level = 60,   /* 默认 60% 亮度，比满亮度省电且不刺眼 */
    .voltage_text = "12.6V",
    .angle_text = "0.0",
    .speed_text = "0"
};

/* ------------------------------------------------------------------ 路由 ---- */

static const KK_UI_PageRoute s_routes[] = {
    { KK_UI_PAGE_HOME, 0U },   /* 页面 1：首页表下标 0。 */
    { KK_UI_PAGE_MENU, 0U },   /* 页面 2：菜单表下标 0。 */
    { KK_UI_PAGE_INFO, 0U },   /* 页面 3：信息表下标 0。 */
    { KK_UI_PAGE_CUSTOM, 0U }  /* 页面 4：自定义页下标 0。 */
};

/* ------------------------------------------------------------------ 首页 ---- */

static const KK_UI_HomeItem s_home_items[] = {
    { "菜单", kk_ui_icon_menu, KK_UI_APP_PAGE_MENU },
    { "状态", kk_ui_icon_status, KK_UI_APP_PAGE_STATUS },
    { "波形", kk_ui_icon_wave, KK_UI_APP_PAGE_WAVE }
};

static const KK_UI_HomePage s_home_pages[] = {
    { s_home_items, 3U }
};

/* ------------------------------------------------------------------ 菜单 ---- */

static const KK_UI_MenuItem s_menu_items[] = {
    { "蜂鸣器", KK_UI_MENU_BOOL, 0U },
    { "限速", KK_UI_MENU_INT, 0U },
    { "亮度", KK_UI_MENU_INT, 1U },
    { "重置", KK_UI_MENU_CONFIRM, 0U }
};

/*
 * 菜单项动态状态表：每项 2 bit，00 正常、01 隐藏、10 禁用。
 * 4 项需要 (4 + 3) / 4 = 1 字节，全零表示全部正常。
 * 业务代码可以调用 KK_UI_SetMenuItemState() 修改它，改完调用 KK_UI_Invalidate()。
 */
static uint8_t s_menu_item_states[(4U + 3U) / 4U];

static const KK_UI_MenuPage s_menu_pages[] = {
    { "菜单", s_menu_items, s_menu_item_states, 4U }
};

/* -------------------------------------------------------------- 信息页 ---- */

/*
 * 信息行的值指向 kk_ui_app_state 里的字符缓冲区，不是编译期常量，
 * 所以这里先只声明数组，字段在 KK_UI_AppInit() 里接上。
 */
static KK_UI_InfoRow s_status_rows[3];

static const KK_UI_InfoPage s_info_pages[] = {
    { "状态", s_status_rows, 3U }
};

/* -------------------------------------------------------------- 变量绑定 ---- */

/*
 * 整数绑定。下标 0 = 限速，下标 1 = 亮度，和菜单项的 ref 一一对应。
 *
 * 亮度下限故意不是 0：设成 0 以后背光全灭，用户看不见界面，
 * 只能盲拨车轮才能把它调回来。留 10% 作为下限避免把自己锁在外面。
 */
static const KK_UI_IntBinding s_int_bindings[] = {
    { "限速", &kk_ui_app_state.speed_limit, 0, 100, 5U, "%",
      KK_UI_APP_EVENT_LIMIT_CHANGED },
    { "亮度", &kk_ui_app_state.backlight_level, 10, 100, 5U, "%",
      KK_UI_APP_EVENT_BACKLIGHT_CHANGED }
};

static const KK_UI_BoolBinding s_bool_bindings[] = {
    { "蜂鸣器", &kk_ui_app_state.buzzer_on, KK_UI_APP_EVENT_BUZZER_CHANGED }
};

static const KK_UI_ConfirmDesc s_confirm_descs[] = {
    { "确定重置", KK_UI_APP_EVENT_RESET_CONFIRMED, KK_UI_EVENT_NONE }
};

/* ------------------------------------------------------- 固定文字与字体 ---- */

/* 字段顺序：返回、取消、确定、开、关、消息框标题。 */
static const KK_UI_Texts s_texts = { "返回", "取消", "确定", "开", "关", "提示" };

/*
 * 首页标签、标题和正文都用同一份 16 像素字模：这份字模同时含可打印 ASCII
 * 和界面用到的中文字，正好覆盖最小样例的全部文字。
 */
static const KK_UI_Fonts s_fonts = {
    kk_ui_font_zh16, kk_ui_font_zh16, kk_ui_font_zh16
};

/* ------------------------------------------------------------------ 清单 ---- */

static const KK_UI_App s_app = {
    .root_page = KK_UI_APP_PAGE_HOME,
    .routes = s_routes,
    .route_count = sizeof(s_routes) / sizeof(s_routes[0]),
    .home_pages = s_home_pages,
    .home_page_count = sizeof(s_home_pages) / sizeof(s_home_pages[0]),
    .menu_pages = s_menu_pages,
    .menu_page_count = sizeof(s_menu_pages) / sizeof(s_menu_pages[0]),
    .info_pages = s_info_pages,
    .info_page_count = sizeof(s_info_pages) / sizeof(s_info_pages[0]),
    .custom_page_count = 1U,
    .int_bindings = s_int_bindings,
    .int_binding_count = sizeof(s_int_bindings) / sizeof(s_int_bindings[0]),
    .bool_bindings = s_bool_bindings,
    .bool_binding_count = sizeof(s_bool_bindings) / sizeof(s_bool_bindings[0]),
    .confirm_descs = s_confirm_descs,
    .confirm_desc_count = sizeof(s_confirm_descs) / sizeof(s_confirm_descs[0]),
    .fonts = s_fonts,
    .texts = s_texts
};

/* ------------------------------------------------------ 波形自定义页 ---- */

/*
 * 波形区几何。所有横向元素都按这几个常量对齐，改动时一起改，避免有的画到框外：
 *   边框外沿 x = 5..122，内侧可用 x = 6..121（正好 KK_UI_WAVE_POINTS 像素）
 *   边框外沿 y = 24..103，内侧可用 y = 25..102
 */
#define KK_UI_WAVE_LEFT 5              /**< 波形区左边（含边框）。 */
#define KK_UI_WAVE_OUTER_WIDTH 118U    /**< 波形区外沿宽度。 */
#define KK_UI_WAVE_TOP 24              /**< 波形区上边。 */
#define KK_UI_WAVE_HEIGHT 80           /**< 波形区高度。 */
#define KK_UI_WAVE_INNER_LEFT 6        /**< 边框内侧左边。 */
#define KK_UI_WAVE_POINTS 116U         /**< 波形点数，正好铺满边框内侧。 */
#define KK_UI_WAVE_RIGHT 122           /**< 波形区右边（含边框），右对齐文字用。 */
#define KK_UI_WAVE_FULL 32    /**< 采样满量程，绘制时按比例折算成像素。 */
#define KK_UI_WAVE_PERIOD_MS 40U /**< 采样间隔，决定波形滚动速度。 */
#define KK_UI_WAVE_AMPLITUDE_MIN 2U
#define KK_UI_WAVE_AMPLITUDE_MAX 16U

/*
 * 波形页是否启用局部重绘。
 *
 * 1 = 每帧只清除并重画波形带，标题、边框和「限速」提示沿用上一帧；
 * 0 = 每帧整屏重画（改动前的行为）。
 * 实物上若发现波形区有残影或与边框错位，把它改成 0 即可回退。
 *
 * 注意：进度条与「限速」文字目前不会变，所以不在脏区里。
 * 一旦它们随数据变化（例如限速值可调），必须把它们的区域也声明进去，
 * 否则旧内容会一直留在屏幕上。
 */
#define KK_UI_APP_WAVE_PARTIAL 1

static uint8_t s_wave_samples[KK_UI_WAVE_POINTS];
static uint16_t s_wave_head;        /**< 下一个写入位置。 */
static uint16_t s_wave_phase;       /**< 占位波形的相位。 */
static uint8_t s_wave_amplitude = 12U;
static uint32_t s_wave_next_ms;     /**< 下一次采样的时刻。 */

/*
 * 占位波形：由相位推出的三角波。
 *
 * 业务模块接入后把这里换成真实采样（例如陀螺仪原始值或 PWM 输出），
 * 只需要把结果归一到 0..KK_UI_WAVE_FULL 并由 KK_UI_CustomOnTick() 推进。
 */
static uint8_t kk_ui_wave_placeholder(void)
{
    uint16_t t = (uint16_t)(s_wave_phase & 0x3FU);
    int32_t value = (t < 32U) ? (int32_t)t : (int32_t)(63U - t);
    value = value * (int32_t)s_wave_amplitude / 32;
    s_wave_phase = (uint16_t)((s_wave_phase + 1U) & 0x3FU);
    if (value < 0) {
        value = 0;
    }
    if (value > KK_UI_WAVE_FULL) {
        value = KK_UI_WAVE_FULL;
    }
    return (uint8_t)value;
}

static void kk_ui_wave_draw_label(int16_t x_offset, int16_t clip_x,
                                  uint16_t clip_width, int16_t y,
                                  const char *text)
{
    int16_t width;
    int16_t x;

    if (text == NULL) {
        return;
    }
    width = (int16_t)TFT_GetUTF8Width(text);
    x = (int16_t)(x_offset + (128 - width) / 2);
    if ((int32_t)x + width > clip_x && x < (int16_t)(clip_x + clip_width)) {
        TFT_DrawUTF8(x, y, text);
    }
}

/* 右对齐到波形区右边沿，用于放置退出提示。 */
static void kk_ui_wave_draw_right(int16_t x_offset, int16_t clip_x,
                                  uint16_t clip_width, int16_t y,
                                  const char *text)
{
    int16_t width;
    int16_t x;

    if (text == NULL) {
        return;
    }
    width = (int16_t)TFT_GetUTF8Width(text);
    x = (int16_t)(x_offset + KK_UI_WAVE_RIGHT - width);
    if ((int32_t)x + width > clip_x && x < (int16_t)(clip_x + clip_width)) {
        TFT_DrawUTF8(x, y, text);
    }
}

/* 波形带（含边框）是这一页唯一会逐帧变化的区域。 */
static void kk_ui_wave_invalidate_plot(void)
{
#if KK_UI_APP_WAVE_PARTIAL
    KK_UI_InvalidateRegion(KK_UI_WAVE_LEFT, KK_UI_WAVE_TOP,
                           KK_UI_WAVE_OUTER_WIDTH, KK_UI_WAVE_HEIGHT);
#endif
}

void KK_UI_CustomOnEnter(KK_UI_PageId page)
{
    uint16_t i;

    if (page != KK_UI_APP_PAGE_WAVE) {
        return;
    }
    for (i = 0U; i < KK_UI_WAVE_POINTS; ++i) {
        s_wave_samples[i] = 0U;
    }
    s_wave_head = 0U;
    s_wave_phase = 0U;
    s_wave_next_ms = 0U;
}

void KK_UI_CustomOnLeave(KK_UI_PageId page)
{
    /* 波形页没有需要释放的资源。 */
    (void)page;
}

void KK_UI_CustomOnInput(KK_UI_PageId page, KK_UI_InputEvent event)
{
    if (page != KK_UI_APP_PAGE_WAVE) {
        return;
    }
    if (event.action == KK_UI_INPUT_OK) {
        /* 在适配函数里发起的提示会在返回后由核心执行。 */
        (void)KK_UI_ShowToast("波形", 800U);
        return;
    }
    if (event.action == KK_UI_INPUT_UP) {
        if (s_wave_amplitude + event.steps <= KK_UI_WAVE_AMPLITUDE_MAX) {
            s_wave_amplitude = (uint8_t)(s_wave_amplitude + event.steps);
        } else {
            s_wave_amplitude = KK_UI_WAVE_AMPLITUDE_MAX;
        }
    } else if (s_wave_amplitude > KK_UI_WAVE_AMPLITUDE_MIN + event.steps) {
        s_wave_amplitude = (uint8_t)(s_wave_amplitude - event.steps);
    } else {
        s_wave_amplitude = KK_UI_WAVE_AMPLITUDE_MIN;
    }
    /* 幅度只影响波形带。 */
    kk_ui_wave_invalidate_plot();
}

bool KK_UI_CustomOnTick(KK_UI_PageId page, uint32_t now_ms)
{
    uint32_t missed;
    uint32_t count;

    if (page != KK_UI_APP_PAGE_WAVE) {
        return false;
    }
    if (s_wave_next_ms == 0U) {
        s_wave_next_ms = now_ms;
    }
    if ((int32_t)(now_ms - s_wave_next_ms) < 0) {
        return false;
    }
    /* 掉帧时只补齐到当前时刻，不累积需要重放的采样。 */
    missed = (now_ms - s_wave_next_ms) / KK_UI_WAVE_PERIOD_MS + 1U;
    if (missed > KK_UI_WAVE_POINTS) {
        missed = KK_UI_WAVE_POINTS;
    }
    count = missed;
    while (count-- != 0U) {
        s_wave_samples[s_wave_head] = kk_ui_wave_placeholder();
        s_wave_head = (uint16_t)((s_wave_head + 1U) % KK_UI_WAVE_POINTS);
    }
    s_wave_next_ms += missed * KK_UI_WAVE_PERIOD_MS;
    /* 只有波形带在逐帧变化，标题、边框与限速提示沿用上一帧。 */
    kk_ui_wave_invalidate_plot();
    return true;
}

void KK_UI_CustomOnDraw(KK_UI_PageId page, int16_t x_offset, int16_t clip_x,
                        uint16_t clip_width)
{
    uint16_t height = TFT_GetHeight();
    int16_t clip_right = (int16_t)(clip_x + clip_width);
    /* 核心是否正在做局部重绘（是则裁剪窗口已由核心按脏矩形设好）。 */
    bool partial = KK_UI_GetDirtyRect(NULL, NULL, NULL, NULL);
    uint16_t i;

    if (page != KK_UI_APP_PAGE_WAVE) {
        return;
    }

    TFT_SetDrawMode(TFT_DRAW_SET);
    TFT_SetBackgroundMode(TFT_BG_TRANSPARENT);
    TFT_SetFontDirection(TFT_ROTATION_0);
    TFT_SetFont(kk_ui_font_zh16);
    TFT_SetFontPosition(TFT_FONT_POS_TOP);
    TFT_SetFontRefHeight(TFT_FONT_REF_ALL);
    /*
     * 局部重绘时必须沿用核心设好的裁剪窗口，不能改成整屏：
     * 否则会把绘制铺满整屏，破坏矩形之外继承下来的上一帧像素。
     */
    if (!partial) {
        TFT_SetClipWindow(clip_x, 0, clip_width, height);
    }

    kk_ui_wave_draw_label(x_offset, clip_x, clip_width, 1, "波形");
    /* 右上角退出提示：左轮后退（返回手势）即回到上一级。 */
    kk_ui_wave_draw_right(x_offset, clip_x, clip_width, 1, "返回");
    TFT_DrawHLine((int16_t)(KK_UI_WAVE_LEFT + x_offset), 18,
                  KK_UI_WAVE_OUTER_WIDTH);

    /* 波形区边框。 */
    TFT_DrawFrame((int16_t)(KK_UI_WAVE_LEFT + x_offset), KK_UI_WAVE_TOP,
                  KK_UI_WAVE_OUTER_WIDTH, KK_UI_WAVE_HEIGHT);

    for (i = 0U; i + 1U < KK_UI_WAVE_POINTS; ++i) {
        uint16_t index0 = (uint16_t)((s_wave_head + i) % KK_UI_WAVE_POINTS);
        uint16_t index1 = (uint16_t)((index0 + 1U) % KK_UI_WAVE_POINTS);
        int16_t x = (int16_t)(KK_UI_WAVE_INNER_LEFT + x_offset + i);
        int16_t y0 = (int16_t)(KK_UI_WAVE_TOP + KK_UI_WAVE_HEIGHT - 2 -
                               (s_wave_samples[index0] *
                                (KK_UI_WAVE_HEIGHT - 3)) / KK_UI_WAVE_FULL);
        int16_t y1 = (int16_t)(KK_UI_WAVE_TOP + KK_UI_WAVE_HEIGHT - 2 -
                               (s_wave_samples[index1] *
                                (KK_UI_WAVE_HEIGHT - 3)) / KK_UI_WAVE_FULL);
        if (x + 1 < clip_x || x > clip_right) {
            continue;
        }
        TFT_DrawLine(x, y0, (int16_t)(x + 1), y1);
        TFT_DrawPixel(x, y1);
    }

    /* 无状态绘制辅助：进度条直接复用菜单里的限速绑定值。 */
    KK_UI_DrawProgressBar((int16_t)(KK_UI_WAVE_INNER_LEFT + x_offset), 112,
                          KK_UI_WAVE_POINTS, 14U,
                          (uint16_t)kk_ui_app_state.speed_limit, 100U);
    kk_ui_wave_draw_label(x_offset, clip_x, clip_width, 132, "限速");

    TFT_ResetClipWindow();
}

/* ------------------------------------------------------------- 生命周期 ---- */

/*
 * 界面活动状态：0 = 还没应用过，1 = 活动（背光亮、UI 推进），2 = 锁定（电机在转）。
 * 用 0 起步是为了让第一次 AppUpdate() 无论实际状态如何都会应用一次背光。
 */
static uint8_t s_ui_state;

/* 把当前亮度写进背光：界面活动时用状态里的亮度，否则熄灭。 */
static void kk_ui_app_apply_backlight(void)
{
    uint8_t level = 0U;

    if (s_ui_state == 1U) {
        int32_t wanted = kk_ui_app_state.backlight_level;

        if (wanted < 0) {
            wanted = 0;
        }
        if (wanted > (int32_t)TFT_BACKLIGHT_LEVEL_MAX) {
            wanted = (int32_t)TFT_BACKLIGHT_LEVEL_MAX;
        }
        level = (uint8_t)wanted;
    }
    /* 背光不经过总线，不需要等异步刷新结束。 */
    (void)TFT_SetBacklightLevel(level);
}

KK_UI_Status KK_UI_AppInit(void)
{
    KK_UI_Status status;

    /* 信息行的值指向长期有效的应用缓冲区，所以在这里接上。 */
    s_status_rows[0].name = "电压";
    s_status_rows[0].value = kk_ui_app_state.voltage_text;
    s_status_rows[1].name = "角度";
    s_status_rows[1].value = kk_ui_app_state.angle_text;
    s_status_rows[2].name = "速度";
    s_status_rows[2].value = kk_ui_app_state.speed_text;

    KK_UI_InputReset();
    status = KK_UI_Init(&s_app);

    /*
     * 立刻按当前阶段设一次背光。
     * 不这样做的话，TFT_DriverInit() 结尾点亮的满亮度会一直持续到主循环里
     * 第一次 KK_UI_AppUpdate()，而两者之间可能隔着几秒的初始化延时。
     */
    s_ui_state = KK_UI_InputWheelUsable() ? 1U : 2U;
    kk_ui_app_apply_backlight();

    return status;
}

void KK_UI_AppUpdate(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t ui_state;
    KK_UI_Input input;
    KK_UI_EventId event;
    KK_UI_ErrorInfo error;

    /*
     * 两个阶段（判据由 KK_UI_Input 提供，见 KK_UI_Input.h 的 KK_UI_INPUT_HAS_WHEEL）：
     *   车轮可用（电机已停止输出）→ 背光点亮，界面正常工作；
     *   车轮不可用（电机在驱动车轮）→ 车轮不是用户输入，关背光节能，
     *                                  并且完全不推进 UI（不取样、不绘制、不提交）。
     * 只在状态切换时写背光：TFT_SetBacklightLevel() 会覆盖 TFT_GetLastStatus()，
     * 每轮都写会把异步刷新的错误状态冲掉。
     */
    ui_state = KK_UI_InputWheelUsable() ? 1U : 2U;
    if (ui_state != s_ui_state) {
        s_ui_state = ui_state;
        kk_ui_app_apply_backlight();
    }
    if (ui_state != 1U) {
        return;
    }

    input = KK_UI_InputRead(now);

    /*
     * 左轮后退 = 取消/返回：有弹框先关弹框（编辑器丢弃草稿、确认框按取消、
     * 提示框按知道了），否则退回上一页。已在根页面时会返回 KK_UI_NOT_ALLOWED，
     * 页面切换动画未结束时返回 KK_UI_BUSY，这两种情况下手势直接丢弃。
     */
    if (KK_UI_InputTakeBackRequest()) {
        (void)KK_UI_RequestBack();
    }

    (void)KK_UI_Update(now, input);

    while (KK_UI_PollEvent(&event)) {
        switch ((KK_UI_AppEvent)event) {
        case KK_UI_APP_EVENT_BUZZER_CHANGED:
            /* 业务模块在这里同步蜂鸣器硬件，真正停转前不要动电机。 */
            break;
        case KK_UI_APP_EVENT_LIMIT_CHANGED:
            /* 业务模块在这里同步限速，并按需要持久化。 */
            break;
        case KK_UI_APP_EVENT_BACKLIGHT_CHANGED:
            /* 亮度是界面自己的表现，改完立即生效（需要重新绘制进度条）。 */
            kk_ui_app_apply_backlight();
            break;
        case KK_UI_APP_EVENT_RESET_CONFIRMED:
            /* 业务模块在这里执行重置。 */
            break;
        default:
            break;
        }
    }

    if (KK_UI_PollError(&error)) {
        /* 显示层出错：尝试恢复；核心正忙时本次会失败，下一次更新再试。 */
        (void)KK_UI_RecoverDisplay();
    }
}

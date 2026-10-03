#include "KK_UI_App.h"

#include "KK_UI_AppIcons.h"
#include "KK_UI_Input.h"
#include "KK_UI_Draw.h"
#include "KK_UI_FontZh16.h"
#include "Analog.h"
#include "TFT.h"
#include "TFT_BootSplash.h"
#include "main.h"

/* KK_UI 可运行样例。只描述页面/绑定/业务事件，刷新所有权归 KK_UI（不调 TFT_Clear/Update）。
 * 页面（5）：1 首页->四个图标入口 | 2 菜单->蜂鸣器/限速/重置 | 3 状态->只读信息行
 *            4 波形(CUSTOM) | 5 图片(CUSTOM，彩色直推)
 */

/* --------------------------------------------------------------- 页面编号 ---- */

enum {
    KK_UI_APP_PAGE_HOME = 1, /**< 首页，同时也是根页面。 */
    KK_UI_APP_PAGE_MENU,     /**< 普通菜单页。 */
    KK_UI_APP_PAGE_STATUS,   /**< 只读信息页。 */
    KK_UI_APP_PAGE_WAVE,     /**< 波形全屏自定义页。 */
    KK_UI_APP_PAGE_IMAGE     /**< 图片全屏自定义页（彩色直推）。 */
};

/* --------------------------------------------------------------- 展示状态 ---- */

KK_UI_AppState kk_ui_app_state = {
    .buzzer_on = true,
    .speed_limit = 60,
    .backlight_level = 60,   /* 默认 60% 亮度，比满亮度省电且不刺眼 */
    /* 仅上电占位；屏幕亮起后由 kk_ui_app_update_voltage() 每 500 ms 覆写。 */
    .voltage_text = "12.6V",
    .angle_text = "0.0",
    .speed_text = "0"
};

/* ------------------------------------------------------------------ 路由 ---- */

static const KK_UI_PageRoute s_routes[] = {
    { KK_UI_PAGE_HOME, 0U },   /* 页面 1：首页表下标 0。 */
    { KK_UI_PAGE_MENU, 0U },   /* 页面 2：菜单表下标 0。 */
    { KK_UI_PAGE_INFO, 0U },   /* 页面 3：信息表下标 0。 */
    { KK_UI_PAGE_CUSTOM, 0U }, /* 页面 4：自定义页下标 0（波形）。 */
    { KK_UI_PAGE_CUSTOM, 1U }  /* 页面 5：自定义页下标 1（图片）。 */
};

/* ------------------------------------------------------------------ 首页 ---- */

static const KK_UI_HomeItem s_home_items[] = {
    { "菜单", kk_ui_icon_menu, KK_UI_APP_PAGE_MENU },
    { "状态", kk_ui_icon_status, KK_UI_APP_PAGE_STATUS },
    { "波形", kk_ui_icon_wave, KK_UI_APP_PAGE_WAVE },
    { "图片", kk_ui_icon_image, KK_UI_APP_PAGE_IMAGE }
};

static const KK_UI_HomePage s_home_pages[] = {
    { s_home_items, 4U }
};

/* ------------------------------------------------------------------ 菜单 ---- */

static const KK_UI_MenuItem s_menu_items[] = {
    { "蜂鸣器", KK_UI_MENU_BOOL, 0U },
    { "限速", KK_UI_MENU_INT, 0U },
    { "亮度", KK_UI_MENU_INT, 1U },
    { "重置", KK_UI_MENU_CONFIRM, 0U }
};

/* 菜单项状态表：每项 2 bit（00 正常 / 01 隐藏 / 10 禁用）；4 项 = 1 字节。
 * 用 KK_UI_SetMenuItemState() 改，改完调 KK_UI_Invalidate()。 */
static uint8_t s_menu_item_states[(4U + 3U) / 4U];

static const KK_UI_MenuPage s_menu_pages[] = {
    { "菜单", s_menu_items, s_menu_item_states, 4U }
};

/* -------------------------------------------------------------- 信息页 ---- */

/* 值指向 kk_ui_app_state 的缓冲区（非编译期常量），字段在 KK_UI_AppInit() 里接上。 */
static KK_UI_InfoRow s_status_rows[3];

static const KK_UI_InfoPage s_info_pages[] = {
    { "状态", s_status_rows, 3U }
};

/* ------------------------------------------------------- 状态页实时数据 ---- */

/* 电压行刷新周期。ADC 在 DMA 里采，这里只读内存 + 格式化；
 * 不限周期的话主循环每轮（~5 ms）都会重绘，500 ms 既能跟上电压变化又把重绘压到 2 Hz。 */
#define KK_UI_APP_VOLTAGE_PERIOD_MS 500U

/** 下一次允许刷新电压的时刻；0 表示还没刷新过，第一次调用立即生效。 */
static uint32_t s_voltage_next_ms;

/** 判断两个 NUL 结尾字符串是否相同（只为这里一次比较，不引 <string.h>）。 */
static bool kk_ui_app_text_equal(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

/** 把无符号整数按十进制写进 out，返回写入的字符数（不含结尾 NUL）。 */
static uint32_t kk_ui_app_put_u32(char *out, uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;
    uint32_t i;

    do {
        digits[count] = (char)('0' + (value % 10U));
        ++count;
        value /= 10U;
    } while (value != 0U);

    for (i = 0U; i < count; ++i) {
        out[i] = digits[count - 1U - i];
    }
    return count;
}

/* 拼出「12.34V」。不能用 %f / %u：nano 版无浮点 printf，整数 printf 会拉进约 3.4 KB stdio。 */
static void kk_ui_app_format_voltage(char *out, int32_t millivolts)
{
    uint32_t mv;
    uint32_t pos;

    if (millivolts < 0) {
        millivolts = 0;
    }
    mv = (uint32_t)millivolts;

    pos = kk_ui_app_put_u32(out, mv / 1000U);
    out[pos] = '.';
    out[pos + 1U] = (char)('0' + ((mv % 1000U) / 100U));
    out[pos + 2U] = (char)('0' + ((mv % 100U) / 10U));
    out[pos + 3U] = 'V';
    out[pos + 4U] = '\0';
}

/* 把电压写进状态页。只在显示文本变化时重绘：否则 ADC 噪声会让整屏一直闪。 */
static void kk_ui_app_update_voltage(uint32_t now_ms)
{
    char text[sizeof(kk_ui_app_state.voltage_text)];
    int32_t millivolts;
    uint32_t i;

    if (s_voltage_next_ms != 0U &&
        (int32_t)(now_ms - s_voltage_next_ms) < 0) {
        return;
    }
    s_voltage_next_ms = now_ms + KK_UI_APP_VOLTAGE_PERIOD_MS;

    /* float -> 整数毫伏，+0.5 四舍五入；有 FPU，且只在主循环里跑。 */
    millivolts = (int32_t)(Analog_ReadVoltage() * 1000.0f + 0.5f);
    kk_ui_app_format_voltage(text, millivolts);

    if (kk_ui_app_text_equal(text, kk_ui_app_state.voltage_text)) {
        return;
    }
    for (i = 0U; i < sizeof(text); ++i) {
        kk_ui_app_state.voltage_text[i] = text[i];
        if (text[i] == '\0') {
            break;
        }
    }
    KK_UI_Invalidate();
}

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
    .custom_page_count = 2U,
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

/* 波形区几何，改动时一起改：边框外沿 x=5..122 / y=24..103，内侧 x=6..121 正好 KK_UI_WAVE_POINTS 像素。 */
#define KK_UI_WAVE_LEFT 5              /**< 波形区左边（含边框）。 */
#define KK_UI_WAVE_OUTER_WIDTH 118U    /**< 波形区外沿宽度。 */
#define KK_UI_WAVE_TOP 24              /**< 波形区上边。 */
#define KK_UI_WAVE_HEIGHT 80           /**< 波形区高度。 */
#define KK_UI_WAVE_INNER_LEFT 6        /**< 边框内侧左边。 */
#define KK_UI_WAVE_POINTS 116U         /**< 波形点数，正好铺满边框内侧。 */
#define KK_UI_WAVE_RIGHT 122           /**< 波形区右边（含边框），右对齐文字用。 */
#define KK_UI_WAVE_PERIOD_MS 40U /**< 采样间隔，决定波形滚动速度。 */

/* 波形纵轴固定 11.1~12.6V，不自动缩放：曲线纵向位置即电压绝对值；超出压到框底/顶。 */
#define KK_UI_WAVE_MIN_MV 11100
#define KK_UI_WAVE_MAX_MV 12600
#define KK_UI_WAVE_SPAN_MV (KK_UI_WAVE_MAX_MV - KK_UI_WAVE_MIN_MV)
#define KK_UI_WAVE_FULL 255U   /**< 归一化满量程，对应 KK_UI_WAVE_MAX_MV。 */

/* 波形页局部重绘：1 = 每帧只重画波形带；0 = 整屏重画（残留/错位时回退用）。
 * 进度条与「限速」文字目前不变所以不在脏区，一旦会变必须一并声明，否则留残影。 */
#define KK_UI_APP_WAVE_PARTIAL 1

static uint8_t s_wave_samples[KK_UI_WAVE_POINTS];
static uint16_t s_wave_head;        /**< 下一个写入位置。 */
static uint32_t s_wave_next_ms;     /**< 下一次采样的时刻。 */

/* 把当前电压折算成 0..KK_UI_WAVE_FULL 的纵向位置。
 * 用 Analog_ReadVoltageCode()（块平均、不过 IIR）；不要改成单点，单点噪声会显示成锯齿。 */
static uint8_t kk_ui_wave_sample(void)
{
    const float volts = (float)Analog_ReadVoltageCode() * VOLTAGE_RATIO /
                        ADC_FULL_SCALE;
    const int32_t mv = (int32_t)(volts * 1000.0f + 0.5f);
    int32_t level;

    if (mv <= KK_UI_WAVE_MIN_MV) {
        return 0U;
    }
    if (mv >= KK_UI_WAVE_MAX_MV) {
        return (uint8_t)KK_UI_WAVE_FULL;
    }
    /* 整数算不溢出；KK_UI_WAVE_FULL 不能超 255（s_wave_samples 是 uint8_t[]）。 */
    level = (mv - KK_UI_WAVE_MIN_MV) * (int32_t)KK_UI_WAVE_FULL /
            KK_UI_WAVE_SPAN_MV;
    return (uint8_t)level;
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

/* -------------------------------------------------------------- 图片自定义页 ---- */

/*
 * 图片页的当前索引与「待推送」标志。
 *
 * 图片是 RGB565 整屏彩色，核心的 1bpp 帧缓冲表达不了，所以这一页走
 * KK_UI_SetDirectFrame(true) 让核心放手，再由这里自己推给面板。
 * 也正因为核心不参与，切图片不需要让核心变脏，只要重推一次。
 */
static uint8_t s_image_index;
static bool s_image_pending;   /**< 需要（重新）把 s_image_index 推上屏。 */

/** 把当前图片推上屏；面板正忙（上一帧还在发）时保留 pending，下一轮再试。 */
static void kk_ui_image_flush(void)
{
    if (!s_image_pending) {
        return;
    }
    if (TFT_BootSplashShow(s_image_index) == TFT_OK) {
        s_image_pending = false;
    }
}

/** 按 steps 切图片，**循环**：越过两端回到另一端。 */
static void kk_ui_image_step(int16_t steps)
{
    int16_t count = (int16_t)TFT_BOOT_SPLASH_COUNT;
    int16_t next;

    if (count <= 0 || steps == 0) {
        return;
    }

    /* 先取模再补回正数：C 的 % 对负数得负结果，直接拿去索引会越界。 */
    next = (int16_t)(((int16_t)s_image_index + steps) % count);
    if (next < 0) {
        next = (int16_t)(next + count);
    }

    if (next != (int16_t)s_image_index) {
        s_image_index = (uint8_t)next;
        s_image_pending = true;
    }
}

void KK_UI_CustomOnEnter(KK_UI_PageId page)
{
    uint16_t i;

    if (page == KK_UI_APP_PAGE_IMAGE) {
        /*
         * 让核心放手：它的帧缓冲只有黑白两色，画不出彩色照片，
         * 而且它每次绘制都会把刚推上去的图片覆盖掉。
         */
        KK_UI_SetDirectFrame(true);
        s_image_pending = true;
        /*
         * 立刻试一次：切页动画期间核心不会调 KK_UI_CustomOnTick()，
         * 不在这里试的话动画那几百毫秒屏幕上还是上一页的旧画面。
         * 推不上去（面板正忙）就交给 Tick 重试。
         */
        kk_ui_image_flush();
        return;
    }

    if (page != KK_UI_APP_PAGE_WAVE) {
        return;
    }
    /*
     * 用当前电压铺满整条缓冲，而不是填 0：填 0 会让曲线从框底爬上来，
     * 而这一页的纵轴是固定量程，一开始就应该停在真实电平上。
     */
    {
        const uint8_t level = kk_ui_wave_sample();

        for (i = 0U; i < KK_UI_WAVE_POINTS; ++i) {
            s_wave_samples[i] = level;
        }
    }
    s_wave_head = 0U;
    s_wave_next_ms = 0U;
}

void KK_UI_CustomOnLeave(KK_UI_PageId page)
{
    /* 波形页没有需要释放的资源。 */
    if (page == KK_UI_APP_PAGE_IMAGE) {
        /*
         * 交回核心：它内部会强制整屏重画（屏幕内容已被我们改过，
         * 帧缓冲里的差异算不出正确结果），所以这里不需额外处理。
         */
        KK_UI_SetDirectFrame(false);
        s_image_pending = false;
    }
}

void KK_UI_CustomOnInput(KK_UI_PageId page, KK_UI_InputEvent event)
{
    if (page == KK_UI_APP_PAGE_IMAGE) {
        /*
         * 上/下拨轮和确定键都是「下一张」，方向与首页轮播保持一致的手感
         * （首页也是拨动切内容）。**循环**：到末尾继续拨会回到第一张，
         * 所以一直往下滑能一直切换。
         */
        if (event.action == KK_UI_INPUT_UP || event.action == KK_UI_INPUT_OK) {
            kk_ui_image_step((int16_t)event.steps);
        } else if (event.action == KK_UI_INPUT_DOWN) {
            kk_ui_image_step((int16_t)(-(int16_t)event.steps));
        }
        /* 立刻推一次手感更跟手；推不上去则由 Tick 补。 */
        kk_ui_image_flush();
        return;
    }

    if (page != KK_UI_APP_PAGE_WAVE) {
        return;
    }
    if (event.action == KK_UI_INPUT_OK) {
        /* 在适配函数里发起的提示会在返回后由核心执行。 */
        (void)KK_UI_ShowToast("波形", 800U);
        return;
    }
    /*
     * 纵轴量程固定（11.1~12.6V），这一页没有可调项，上/下拨轮不做事。
     * 以后若在这里加可调项，改完同样要调 kk_ui_wave_invalidate_plot()。
     */
}

bool KK_UI_CustomOnTick(KK_UI_PageId page, uint32_t now_ms)
{
    uint32_t missed;
    uint32_t count;

    if (page == KK_UI_APP_PAGE_IMAGE) {
        kk_ui_image_flush();
        /*
         * 返回 false：屏幕内容归这一页自己管，不需要核心变脏重绘。
         * 返回 true 会让核心置脏，反而去重画黑白界面把图片盖掉。
         */
        return false;
    }

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
        s_wave_samples[s_wave_head] = kk_ui_wave_sample();
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

    if (page == KK_UI_APP_PAGE_IMAGE) {
        /*
         * 直推模式下核心不会调本函数；真被调到也不该画任何东西，
         * 否则会把刚推上去的图片覆盖成黑白界面。
         */
        return;
    }

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

/* 0 = 还没应用过，1 = 活动，2 = 锁定（电机在转）。0 起步保证首次 AppUpdate 必应用一次背光。 */
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

    /* 立刻按当前阶段设一次背光，否则 TFT_DriverInit() 的满亮度会持续到首次 AppUpdate。 */
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
     * 车轮可用（电机停）-> 背光点亮、界面正常；不可用 -> 关背光且完全不推进 UI。
     * 只在状态切换时写背光：每轮都写会冲掉 TFT_GetLastStatus() 的异步刷新状态。
     */
    ui_state = KK_UI_InputWheelUsable() ? 1U : 2U;
    if (ui_state != s_ui_state) {
        s_ui_state = ui_state;
        kk_ui_app_apply_backlight();
    }
    if (ui_state != 1U) {
        return;
    }

    /* 状态页的电压是唯一一份需要周期性刷新的业务数据。 */
    kk_ui_app_update_voltage(now);

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

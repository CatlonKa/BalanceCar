#include "KK_UI_Internal.h"

#if KK_UI_ENABLE_HOME

static const KK_UI_HomePage *kk_ui_home_page(const KK_UI_PageState *state)
{
    const KK_UI_PageRoute *route = KK_UI_GetRoute(state->page);
    return &kk_ui.app->home_pages[route->index];
}

/** 选中项居中时，列表应该停在的滚动位置（Q8 像素）。 */
static int32_t kk_ui_home_target_q8(uint16_t selected)
{
    return (int32_t)selected * KK_UI_HOME_ITEM_SPACING * 256;
}

/** 当前滚动位置，Q8 像素；动画进行中按缓出曲线插值。 */
static int32_t kk_ui_home_scroll_q8(uint32_t now)
{
    if (kk_ui.home_anim.active == 0U) {
        return kk_ui_home_target_q8(kk_ui.current.selected);
    }
    return KK_UI_LerpQ12(kk_ui.home_anim.scroll_from_q8,
                         kk_ui_home_target_q8(kk_ui.current.selected),
                         KK_UI_EaseQ12(now - kk_ui.home_anim.started,
                                       KK_UI_HOME_MS));
}

void KK_UI_HomeAnimate(uint32_t now)
{
    if (KK_UI_CurrentPageType() != KK_UI_PAGE_HOME ||
        kk_ui.home_anim.active == 0U) {
        return;
    }
    kk_ui.dirty = 1U;
    if (now - kk_ui.home_anim.started >= KK_UI_HOME_MS) {
        kk_ui.home_anim.active = 0U;
    }
}

/** 已经在两端还往外拨：闪一下整屏边框，和菜单页用同一套机制。 */
static void kk_ui_home_reject(void)
{
    kk_ui.reject_flash = 2U;
    kk_ui.dirty = 1U;
}

static void kk_ui_home_move(int16_t direction, uint16_t steps, uint32_t now)
{
    const KK_UI_HomePage *page = kk_ui_home_page(&kk_ui.current);
    int32_t target;

    if (page->item_count == 0U || steps == 0U || direction == 0) {
        return;
    }

    /* 两端夹紧，不做首尾相接的循环：滚到头就停住并给出拒绝反馈。 */
    target = (int32_t)kk_ui.current.selected +
             (int32_t)direction * (int32_t)steps;
    if (target < 0) {
        target = 0;
        kk_ui_home_reject();
    } else if (target > (int32_t)page->item_count - 1) {
        target = (int32_t)page->item_count - 1;
        kk_ui_home_reject();
    }

    /* 位置没变又不处于动画中，说明这次拨动被夹住了，重开动画只会白跑一遍。 */
    if (target == (int32_t)kk_ui.current.selected &&
        kk_ui.home_anim.active == 0U) {
        return;
    }

    /* 从当前（可能是动画中间）的位置接着走，连拨时才不会跳。 */
    kk_ui.home_anim.scroll_from_q8 = kk_ui_home_scroll_q8(now);
    kk_ui.current.selected = (uint16_t)target;
    kk_ui.home_anim.started = now;
    kk_ui.home_anim.active = 1U;
    kk_ui.dirty = 1U;
}

void KK_UI_HomeInput(KK_UI_InputEvent event, uint32_t now)
{
    const KK_UI_HomePage *page = kk_ui_home_page(&kk_ui.current);

    if (event.action == KK_UI_INPUT_OK) {
        KK_UI_PageId target = page->items[kk_ui.current.selected].target_page;
        kk_ui.home_anim.active = 0U;
        (void)KK_UI_NavigateTo(target, now);
    } else if (event.action == KK_UI_INPUT_UP) {
        /*
         * 拨动方向与菜单相反，这是刻意的。
         *
         * 菜单是列表排版、高亮条在动：向上拨 -> 高亮往上，看起来「内容往上走」。
         * 首页的选中项固定在中间，动的是内容本身；要让手感与菜单一致，
         * 就得让「向上拨 -> 内容往上走」，也就是选中下面那一项。
         *
         * 代价：在列表两端之外多拨一下会被夹住（闪一下边框），
         * 而菜单在两端是同一个方向。需要换回菜单那种对应关系时，
         * 把下面两个分支的 1 / -1 对调即可。
         */
        kk_ui_home_move(1, event.steps, now);
    } else {
        kk_ui_home_move(-1, event.steps, now);
    }
}

/*
 * 画一项。尺寸和横向位置都由「离选中位的距离」推出，距离越近越大、越靠右，
 * 所以换项动画只要滚动位置在动，放大缩小与左右移动就自然跟着走。
 */
static void kk_ui_home_draw_item(const KK_UI_HomePage *page,
                                 const KK_UI_PageState *state,
                                 int32_t scroll_q8, int16_t x_offset,
                                 uint16_t index)
{
    const int32_t span_q8 = (int32_t)KK_UI_HOME_ITEM_SPACING * 256;
    int32_t delta_q8 = (int32_t)index * KK_UI_HOME_ITEM_SPACING * 256 -
                       scroll_q8;
    int32_t distance_q8 = delta_q8 < 0 ? -delta_q8 : delta_q8;
    int32_t level_q8;   /* 256 = 完全选中，0 = 已离一个间距以上。 */
    int16_t center_y;
    int16_t center_x;
    int16_t size;
    int16_t label_width;
    int16_t label_x;
    int16_t label_y;
    int16_t normal_left;
    int16_t wide_left;

    if (distance_q8 > span_q8) {
        distance_q8 = span_q8;
    }
    level_q8 = 256 - (distance_q8 * 256) / span_q8;

    size = (int16_t)(KK_UI_HOME_ICON_SIZE +
           ((KK_UI_HOME_ICON_FOCUS - KK_UI_HOME_ICON_SIZE) * level_q8) / 256);
    center_x = (int16_t)(KK_UI_HOME_ICON_CENTER_X +
               ((KK_UI_HOME_ICON_FOCUS_X - KK_UI_HOME_ICON_CENTER_X) *
                level_q8) / 256);
    center_y = (int16_t)(KK_UI_HOME_CENTER_Y + delta_q8 / 256);

    /* 只画纵向与画布有交集的项；两端被裁掉一半的正是靠这里处理。 */
    if (center_y + size / 2 > 0 && center_y - size / 2 < KK_UI_SCREEN_HEIGHT) {
        TFT_DrawXBMScale(
            (int16_t)(center_x - size / 2 + x_offset),
            (int16_t)(center_y - size / 2),
            KK_UI_HOME_ICON_SIZE, KK_UI_HOME_ICON_SIZE,
            page->items[index].icon_xbm_32x32,
            (uint16_t)size, (uint16_t)size);
    }

    /*
     * 标签在图标右侧、与图标纵向居中。未选中时靠左对齐，选中时右对齐到屏幕
     * 右边，中间按同样的 level 插值，于是它随图标一起平滑地滑过去。
     */
    label_width = (int16_t)TFT_GetUTF8Width(page->items[index].label);
    normal_left = (int16_t)(KK_UI_HOME_TEXT_LEFT + x_offset);
    wide_left = (int16_t)(KK_UI_SCREEN_WIDTH - KK_UI_HOME_TEXT_RIGHT_GAP -
                          label_width + x_offset);
    /* 标签太长时不越过左边界，宁可和图标挤一点。 */
    if (wide_left < KK_UI_HOME_TEXT_LEFT) {
        wide_left = (int16_t)(KK_UI_HOME_TEXT_LEFT + x_offset);
    }
    label_x = (int16_t)(normal_left +
              ((wide_left - normal_left) * level_q8) / 256);
    label_y = (int16_t)(center_y - KK_UI_HOME_LABEL_HEIGHT / 2);

    if (label_y + KK_UI_HOME_LABEL_HEIGHT <= 0 ||
        label_y >= KK_UI_SCREEN_HEIGHT) {
        return;
    }

    if (index == state->selected) {
        /* 反白：先铺实心白框，再用 CLEAR 模式写字，得到白底黑字。 */
        TFT_DrawRBox((int16_t)(label_x - 4), label_y,
                     (uint16_t)(label_width + 8),
                     (uint16_t)KK_UI_HOME_LABEL_HEIGHT, 3U);
        TFT_SetDrawMode(TFT_DRAW_CLEAR);
        TFT_DrawUTF8(label_x, label_y, page->items[index].label);
        TFT_SetDrawMode(TFT_DRAW_SET);
    } else {
        TFT_DrawUTF8(label_x, label_y, page->items[index].label);
    }
}

void KK_UI_HomeDraw(const KK_UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{
    const KK_UI_HomePage *page = kk_ui_home_page(state);
    int32_t scroll_q8;
    uint16_t i;

    TFT_SetClipWindow(clip_x, 0, clip_width, KK_UI_SCREEN_HEIGHT);
    TFT_SetFont(kk_ui.app->fonts.home_font);

    if (state == &kk_ui.current) {
        scroll_q8 = kk_ui_home_scroll_q8(kk_ui.last_update);
    } else {
        /* 切页动画里正在离开的那一页：定格在它自己的选中项上，不再播放动画。 */
        scroll_q8 = kk_ui_home_target_q8(state->selected);
    }

    /*
     * 先画没选中的，最后画选中项：放大的那个会盖住相邻项的一小条边，
     * 这样看起来是「压在前面」的层次，而不是两块图糊在一起。
     */
    for (i = 0U; i < page->item_count; ++i) {
        if (i != state->selected) {
            kk_ui_home_draw_item(page, state, scroll_q8, x_offset, i);
        }
    }
    if (state->selected < page->item_count) {
        kk_ui_home_draw_item(page, state, scroll_q8, x_offset,
                             state->selected);
    }
    TFT_ResetClipWindow();
}

#else
void KK_UI_HomeAnimate(uint32_t now) { (void)now; }
void KK_UI_HomeInput(KK_UI_InputEvent event, uint32_t now)
{ (void)event; (void)now; }
void KK_UI_HomeDraw(const KK_UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{ (void)state; (void)x_offset; (void)clip_x; (void)clip_width; }
#endif

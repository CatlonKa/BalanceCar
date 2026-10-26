#include "KK_UI_Internal.h"

#include <limits.h>

#define KK_UI_MODAL_ENABLED (KK_UI_ENABLE_INT_EDITOR || \
                             KK_UI_ENABLE_BOOL_EDITOR || \
                             KK_UI_ENABLE_ENUM_EDITOR || \
                             KK_UI_ENABLE_CONFIRM || \
                             KK_UI_ENABLE_MESSAGE)

void KK_UI_DrawCentered(int16_t x, int16_t y, uint16_t width,
                        const char *text)
{
    int16_t text_x;
    if (text == NULL) {
        return;
    }
    text_x = (int16_t)(x + ((int32_t)width - TFT_GetUTF8Width(text)) / 2);
    TFT_DrawUTF8(text_x, y, text);
}

void KK_UI_FormatInt(int32_t value, const char *unit, char *buffer,
                     size_t capacity)
{
    char reverse[11];
    uint32_t magnitude;
    size_t digits = 0U;
    size_t out = 0U;

    if (buffer == NULL || capacity == 0U) {
        return;
    }
    if (value < 0) {
        magnitude = (uint32_t)(-(value + 1)) + 1U;
        if (out + 1U < capacity) {
            buffer[out++] = '-';
        }
    } else {
        magnitude = (uint32_t)value;
    }
    do {
        reverse[digits++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U && digits < sizeof(reverse));
    while (digits != 0U && out + 1U < capacity) {
        buffer[out++] = reverse[--digits];
    }
    if (unit != NULL) {
        size_t i = 0U;
        while (unit[i] != '\0' && out + 1U < capacity) {
            buffer[out++] = unit[i++];
        }
    }
    buffer[out] = '\0';
}

bool KK_UI_DialogActive(void)
{
#if KK_UI_MODAL_ENABLED
    return kk_ui.overlay.type != KK_UI_OVERLAY_NONE;
#else
    return false;
#endif
}

#if KK_UI_MODAL_ENABLED
static int16_t kk_ui_dialog_origin(void)
{
    if (KK_UI_CurrentPageType() == KK_UI_PAGE_MENU) {
        /*
         * 参考布局里弹框紧贴菜单焦点行展开，因为焦点行的纵向范围本身
         * 就在弹框最终位置附近。本工程画布更高、可见行更多，焦点行可能
         * 离弹框带很远；只在焦点行确实落在弹框带内时才用它做起点，
         * 否则退回 KK_UI_DIALOG_TOP，避免长距离纵向滑动。
         */
        int16_t row = (int16_t)(KK_UI_MENU_HEADER_HEIGHT +
                                KK_UI_RoundQ8(kk_ui.current.focus_q8));
        if (row >= KK_UI_DIALOG_TOP && row <= KK_UI_DIALOG_TOP + 45) {
            return row;
        }
    }
    return KK_UI_DIALOG_TOP;
}
#endif

#if KK_UI_MODAL_ENABLED
static const KK_UI_EnumBinding *kk_ui_enum_binding(void)
{
    return &kk_ui.app->enum_bindings[kk_ui.overlay.ref];
}

/* 选项数超过上限时按上限画，避免弹框顶出屏幕之外。 */
static uint16_t kk_ui_enum_count(void)
{
    uint16_t count = kk_ui_enum_binding()->option_count;

    return count > (uint16_t)KK_UI_ENUM_MAX_OPTIONS ?
           (uint16_t)KK_UI_ENUM_MAX_OPTIONS : count;
}

/* 列表底边（最后一行框的下边线）相对弹框顶边的偏移 */
static int16_t kk_ui_enum_rows_bottom(void)
{
    return (int16_t)(KK_UI_ENUM_ROWS_TOP +
                     (kk_ui_enum_count() - 1U) * KK_UI_ENUM_ROW_PITCH +
                     KK_UI_ENUM_ROW_HEIGHT);
}

/* 标题 + 列表 + 间隔 + 「确定」按钮 + 下留白 */
static int16_t kk_ui_enum_height(void)
{
    return (int16_t)(kk_ui_enum_rows_bottom() + KK_UI_ENUM_CONFIRM_GAP +
                     KK_UI_ENUM_CONFIRM_HEIGHT + 3);
}

/* 太高就整体上移，底边固定不越出画布 */
static int16_t kk_ui_enum_top(void)
{
    int16_t height = kk_ui_enum_height();
    int16_t top = KK_UI_DIALOG_TOP;

    if (top + height > KK_UI_SCREEN_HEIGHT - 3) {
        top = (int16_t)(KK_UI_SCREEN_HEIGHT - 3 - height);
    }
    return top < 2 ? 2 : top;
}

static int16_t kk_ui_enum_confirm_top(void)
{
    return (int16_t)(kk_ui_enum_top() + kk_ui_enum_rows_bottom() +
                     KK_UI_ENUM_CONFIRM_GAP);
}

static int16_t kk_ui_enum_row_top(uint16_t index)
{
    return (int16_t)(kk_ui_enum_top() + KK_UI_ENUM_ROWS_TOP +
                     index * KK_UI_ENUM_ROW_PITCH);
}

static void kk_ui_dialog_focus_rect(uint8_t focus, int16_t *x, int16_t *y,
                                    int16_t *width, int16_t *height)
{
    if (kk_ui.overlay.type == KK_UI_OVERLAY_MESSAGE) {
        *x = 46; *y = 88; *width = 36; *height = 20;
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_ENUM) {
        /* 列表的焦点用「反白选项」表达，焦点框只服务底部的「确定」。 */
        *x = KK_UI_ENUM_BOX_LEFT; *y = kk_ui_enum_confirm_top();
        *width = KK_UI_ENUM_BOX_WIDTH;
        *height = (int16_t)KK_UI_ENUM_CONFIRM_HEIGHT;
    } else if (focus == KK_UI_FOCUS_VALUE) {
        /*
         * 字模在 POS_TOP/REF_ALL 下实际占 16 行（y+1..y+16），所以框高给 20：
         * 上下各留 2 像素，16 像素的旧框会把最后一行的下缘切掉。
         */
        *y = 68;
        *height = 20;
        if (kk_ui.overlay.type == KK_UI_OVERLAY_BOOL) {
            /* 开关行两侧文字 + 开关图形横跨 15..113，取整行宽度才包得住字。 */
            *x = 15;
            *width = 99;
        } else {
            /* 整数编辑器只框住数值本身，箭头留在框外。 */
            *x = 39;
            *width = 46;
        }
    } else {
        /* 取消已改成左轮后退手势，弹框底部只剩一个整宽的「确定」。
           高度同样给 20，避免末行文字落在反白区外。 */
        *x = 15; *y = 88; *width = 98; *height = 20;
    }
}

static void kk_ui_dialog_current_focus(uint32_t now, int32_t *x, int32_t *y,
                                       int32_t *width, int32_t *height)
{
    if (kk_ui.overlay.focus_moving != 0U) {
        uint16_t p = KK_UI_EaseQ12(now - kk_ui.overlay.focus_started,
                                   KK_UI_FOCUS_MS);
        *x = KK_UI_LerpQ12(kk_ui.overlay.focus_from_x_q8,
                          kk_ui.overlay.focus_x_q8, p);
        *y = KK_UI_LerpQ12(kk_ui.overlay.focus_from_y_q8,
                          kk_ui.overlay.focus_y_q8, p);
        *width = KK_UI_LerpQ12(kk_ui.overlay.focus_from_w_q8,
                              kk_ui.overlay.focus_w_q8, p);
        *height = KK_UI_LerpQ12(kk_ui.overlay.focus_from_h_q8,
                               kk_ui.overlay.focus_h_q8, p);
    } else {
        *x = kk_ui.overlay.focus_x_q8;
        *y = kk_ui.overlay.focus_y_q8;
        *width = kk_ui.overlay.focus_w_q8;
        *height = kk_ui.overlay.focus_h_q8;
    }
}

static void kk_ui_dialog_set_focus(uint8_t focus, uint32_t now)
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    kk_ui_dialog_current_focus(now, &kk_ui.overlay.focus_from_x_q8,
                               &kk_ui.overlay.focus_from_y_q8,
                               &kk_ui.overlay.focus_from_w_q8,
                               &kk_ui.overlay.focus_from_h_q8);
    kk_ui.overlay.focus = focus;
    kk_ui_dialog_focus_rect(focus, &x, &y, &width, &height);
    kk_ui.overlay.focus_x_q8 = (int32_t)x * 256;
    kk_ui.overlay.focus_y_q8 = (int32_t)y * 256;
    kk_ui.overlay.focus_w_q8 = (int32_t)width * 256;
    kk_ui.overlay.focus_h_q8 = (int32_t)height * 256;
    kk_ui.overlay.focus_started = now;
    kk_ui.overlay.focus_moving = 1U;
    kk_ui.dirty = 1U;
}

static KK_UI_Status kk_ui_dialog_begin(KK_UI_OverlayType type, uint16_t ref,
                                       const char *text,
                                       KK_UI_EventId event, uint32_t now)
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    if (KK_UI_DialogActive()) {
        return KK_UI_BUSY;
    }
    kk_ui.toast.visible = 0U;
    kk_ui.overlay.type = type;
    kk_ui.overlay.ref = ref;
    kk_ui.overlay.text = text;
    kk_ui.overlay.event = event;
    kk_ui.overlay.origin_y = kk_ui_dialog_origin();
    kk_ui.overlay.phase_started = now;
    kk_ui.overlay.phase_q12 = 0U;
    kk_ui.overlay.phase_from_q12 = 0U;
    kk_ui.overlay.value_q12 = KK_UI_Q12_ONE;
    kk_ui.overlay.closing = 0U;
    kk_ui.overlay.focus_moving = 0U;
    kk_ui.overlay.value_moving = 0U;
    if (type == KK_UI_OVERLAY_CONFIRM) {
        kk_ui.overlay.focus = KK_UI_FOCUS_CONFIRM;
        kk_ui.overlay.editing = 0U;
    } else if (type == KK_UI_OVERLAY_MESSAGE) {
        kk_ui.overlay.focus = KK_UI_FOCUS_CONFIRM;
        kk_ui.overlay.editing = 0U;
    } else {
        kk_ui.overlay.focus = KK_UI_FOCUS_VALUE;
        kk_ui.overlay.editing = 1U;
    }
    kk_ui_dialog_focus_rect(kk_ui.overlay.focus, &x, &y, &width, &height);
    kk_ui.overlay.focus_x_q8 = kk_ui.overlay.focus_from_x_q8 =
        (int32_t)x * 256;
    kk_ui.overlay.focus_y_q8 = kk_ui.overlay.focus_from_y_q8 =
        (int32_t)y * 256;
    kk_ui.overlay.focus_w_q8 = kk_ui.overlay.focus_from_w_q8 =
        (int32_t)width * 256;
    kk_ui.overlay.focus_h_q8 = kk_ui.overlay.focus_from_h_q8 =
        (int32_t)height * 256;
    kk_ui.dirty = 1U;
    return KK_UI_OK;
}
#endif

KK_UI_Status KK_UI_DialogOpenInt(uint16_t index, uint32_t now)
{
#if KK_UI_ENABLE_INT_EDITOR
    KK_UI_Status status;
    if (index >= kk_ui.app->int_binding_count) {
        return KK_UI_INVALID_ARGUMENT;
    }
    status = kk_ui_dialog_begin(KK_UI_OVERLAY_INT, index, NULL,
                                KK_UI_EVENT_NONE, now);
    if (status == KK_UI_OK) {
        kk_ui.overlay.draft = *kk_ui.app->int_bindings[index].value;
        kk_ui.overlay.original = kk_ui.overlay.draft;
        kk_ui.overlay.previous = kk_ui.overlay.draft;
    }
    return status;
#else
    (void)index; (void)now;
    return KK_UI_UNSUPPORTED;
#endif
}

KK_UI_Status KK_UI_DialogOpenBool(uint16_t index, uint32_t now)
{
#if KK_UI_ENABLE_BOOL_EDITOR
    KK_UI_Status status;
    if (index >= kk_ui.app->bool_binding_count) {
        return KK_UI_INVALID_ARGUMENT;
    }
    status = kk_ui_dialog_begin(KK_UI_OVERLAY_BOOL, index, NULL,
                                KK_UI_EVENT_NONE, now);
    if (status == KK_UI_OK) {
        kk_ui.overlay.draft = *kk_ui.app->bool_bindings[index].value ? 1 : 0;
        kk_ui.overlay.original = kk_ui.overlay.draft;
        kk_ui.overlay.previous = kk_ui.overlay.draft;
    }
    return status;
#else
    (void)index; (void)now;
    return KK_UI_UNSUPPORTED;
#endif
}

KK_UI_Status KK_UI_DialogOpenEnum(uint16_t index, uint32_t now)
{
#if KK_UI_ENABLE_ENUM_EDITOR
    KK_UI_Status status;
    const KK_UI_EnumBinding *binding;

    if (index >= kk_ui.app->enum_binding_count) {
        return KK_UI_INVALID_ARGUMENT;
    }
    binding = &kk_ui.app->enum_bindings[index];
    status = kk_ui_dialog_begin(KK_UI_OVERLAY_ENUM, index, NULL,
                                KK_UI_EVENT_NONE, now);
    if (status == KK_UI_OK) {
        kk_ui.overlay.draft = *binding->value;
        kk_ui.overlay.original = kk_ui.overlay.draft;
        kk_ui.overlay.previous = kk_ui.overlay.draft;
    }
    return status;
#else
    (void)index; (void)now;
    return KK_UI_UNSUPPORTED;
#endif
}

KK_UI_Status KK_UI_DialogOpenConfirm(uint16_t index, uint32_t now)
{
#if KK_UI_ENABLE_CONFIRM
    if (index >= kk_ui.app->confirm_desc_count) {
        return KK_UI_INVALID_ARGUMENT;
    }
    return kk_ui_dialog_begin(KK_UI_OVERLAY_CONFIRM, index,
                              kk_ui.app->confirm_descs[index].text,
                              KK_UI_EVENT_NONE, now);
#else
    (void)index; (void)now;
    return KK_UI_UNSUPPORTED;
#endif
}

KK_UI_Status KK_UI_DialogOpenMessage(const char *text, KK_UI_EventId event,
                                     uint32_t now)
{
#if KK_UI_ENABLE_MESSAGE
    if (text == NULL) {
        return KK_UI_INVALID_ARGUMENT;
    }
    return kk_ui_dialog_begin(KK_UI_OVERLAY_MESSAGE, 0U, text, event, now);
#else
    (void)text; (void)event; (void)now;
    return KK_UI_UNSUPPORTED;
#endif
}

#if KK_UI_MODAL_ENABLED
static void kk_ui_dialog_close(uint32_t now)
{
    kk_ui.overlay.phase_from_q12 = kk_ui.overlay.phase_q12;
    kk_ui.overlay.phase_started = now;
    kk_ui.overlay.closing = 1U;
    kk_ui.overlay.editing = 0U;
    kk_ui.dirty = 1U;
}

static bool kk_ui_dialog_event_or_reject(KK_UI_EventId event)
{
    if (event == KK_UI_EVENT_NONE || KK_UI_QueueEvent(event)) {
        return true;
    }
    KK_UI_RecordError(KK_UI_QUEUE_FULL, kk_ui.current.page, event);
    kk_ui.reject_flash = 2U;
    kk_ui.dirty = 1U;
    return false;
}

void KK_UI_DialogDismiss(uint32_t now)
{
    if (!KK_UI_DialogActive() || kk_ui.overlay.closing != 0U) {
        return;
    }
    /*
     * 后退（取消）：
     *   提示框 -> 等同于「知道了」，照常投递确认后的业务事件；
     *   确认框 -> 投递 cancelled_event；
     *   编辑器 -> 丢弃草稿，不提交、不发任何业务事件。
     */
    if (kk_ui.overlay.type == KK_UI_OVERLAY_MESSAGE) {
        if (!kk_ui_dialog_event_or_reject(kk_ui.overlay.event)) {
            return;
        }
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_CONFIRM) {
        const KK_UI_ConfirmDesc *desc =
            &kk_ui.app->confirm_descs[kk_ui.overlay.ref];
        if (!kk_ui_dialog_event_or_reject(desc->cancelled_event)) {
            return;
        }
    }
    kk_ui_dialog_close(now);
}

static int32_t kk_ui_int_adjust(const KK_UI_IntBinding *binding,
                                int32_t value, bool increase,
                                uint16_t steps)
{
    int64_t next = value;
    int64_t amount = (int64_t)binding->step * steps;
    if (increase) {
        next += amount;
        return next > binding->maximum ? binding->maximum : (int32_t)next;
    }
    next -= amount;
    return next < binding->minimum ? binding->minimum : (int32_t)next;
}

static void kk_ui_dialog_change_value(KK_UI_InputEvent event, uint32_t now)
{
    int32_t next = kk_ui.overlay.draft;
    bool increase = event.action == KK_UI_INPUT_UP;

    if (kk_ui.overlay.type == KK_UI_OVERLAY_INT) {
        const KK_UI_IntBinding *binding =
            &kk_ui.app->int_bindings[kk_ui.overlay.ref];
        next = kk_ui_int_adjust(binding, next, increase, event.steps);
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_BOOL) {
        /* A switch has no directional meaning: either direction toggles it.
         * Ignore key auto-repeat so one long press cannot oscillate the value.
         * Batched encoder detents are equivalent to that many toggles. */
        if (event.source == KK_UI_INPUT_REPEAT || (event.steps & 1U) == 0U) {
            return;
        }
        next = kk_ui.overlay.draft == 0 ? 1 : 0;
        increase = next != 0;
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_ENUM) {
        /* 多选一：向上 = 上一项，向下 = 下一项（与菜单列表同向）。到头就停，
           不循环，避免误转一圈又回到原值。 */
        int32_t last = (int32_t)kk_ui_enum_count() - 1;
        next += (event.action == KK_UI_INPUT_UP) ? -(int32_t)event.steps :
                                                   (int32_t)event.steps;
        if (next < 0) {
            next = 0;
        }
        if (next > last) {
            next = last;
        }
        increase = next > kk_ui.overlay.draft;
    }
    if (next != kk_ui.overlay.draft) {
        kk_ui.overlay.previous = kk_ui.overlay.draft;
        kk_ui.overlay.draft = next;
        kk_ui.overlay.value_direction = increase ? 1 : -1;
        kk_ui.overlay.value_q12 = 0U;
        kk_ui.overlay.value_started = now;
        kk_ui.overlay.value_moving = 1U;
        kk_ui.dirty = 1U;
    }
}

static void kk_ui_dialog_commit(uint32_t now)
{
    KK_UI_EventId event = KK_UI_EVENT_NONE;
    if (kk_ui.overlay.type == KK_UI_OVERLAY_INT) {
        const KK_UI_IntBinding *binding =
            &kk_ui.app->int_bindings[kk_ui.overlay.ref];
        if (kk_ui.overlay.draft != kk_ui.overlay.original) {
            event = binding->changed_event;
            if (!kk_ui_dialog_event_or_reject(event)) {
                return;
            }
            *binding->value = kk_ui.overlay.draft;
        }
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_BOOL) {
        const KK_UI_BoolBinding *binding =
            &kk_ui.app->bool_bindings[kk_ui.overlay.ref];
        if (kk_ui.overlay.draft != kk_ui.overlay.original) {
            event = binding->changed_event;
            if (!kk_ui_dialog_event_or_reject(event)) {
                return;
            }
            *binding->value = kk_ui.overlay.draft != 0;
        }
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_ENUM) {
        const KK_UI_EnumBinding *binding = kk_ui_enum_binding();
        if (kk_ui.overlay.draft != kk_ui.overlay.original) {
            event = binding->changed_event;
            if (!kk_ui_dialog_event_or_reject(event)) {
                return;
            }
            *binding->value = kk_ui.overlay.draft;
        }
    }
    kk_ui_dialog_close(now);
}

static void kk_ui_dialog_confirm_action(uint32_t now)
{
    const KK_UI_ConfirmDesc *desc =
        &kk_ui.app->confirm_descs[kk_ui.overlay.ref];
    KK_UI_EventId event = kk_ui.overlay.focus == KK_UI_FOCUS_CONFIRM ?
                          desc->confirmed_event : desc->cancelled_event;
    if (kk_ui_dialog_event_or_reject(event)) {
        kk_ui_dialog_close(now);
    }
}

void KK_UI_DialogInput(KK_UI_InputEvent event, uint32_t now)
{
    int16_t next;

    if (!KK_UI_DialogActive() || kk_ui.overlay.closing != 0U) {
        return;
    }
    if (kk_ui.overlay.type == KK_UI_OVERLAY_MESSAGE) {
        if (event.action == KK_UI_INPUT_OK &&
            kk_ui_dialog_event_or_reject(kk_ui.overlay.event)) {
            kk_ui_dialog_close(now);
        }
        return;
    }
    if (kk_ui.overlay.type == KK_UI_OVERLAY_CONFIRM) {
        if (event.action == KK_UI_INPUT_OK) {
            kk_ui_dialog_confirm_action(now);
        }
        /* 确认框只剩一个「确定」：取消靠左轮后退手势，转轮不做任何事。 */
        return;
    }
    if (kk_ui.overlay.editing != 0U) {
        if (event.action == KK_UI_INPUT_OK) {
            kk_ui.overlay.editing = 0U;
            kk_ui_dialog_set_focus(KK_UI_FOCUS_CONFIRM, now);
        } else {
            kk_ui_dialog_change_value(event, now);
        }
        return;
    }
    if (event.action == KK_UI_INPUT_OK) {
        if (kk_ui.overlay.focus == KK_UI_FOCUS_VALUE) {
            kk_ui.overlay.editing = 1U;
            kk_ui.dirty = 1U;
        } else if (kk_ui.overlay.focus == KK_UI_FOCUS_CONFIRM) {
            kk_ui_dialog_commit(now);
        }
        return;
    }
    /* 编辑器只有「数值」和「确定」两个落点，取消靠左轮后退手势。 */
    next = (int16_t)kk_ui.overlay.focus +
           (event.action == KK_UI_INPUT_UP ? -1 : 1);
    if (next < KK_UI_FOCUS_VALUE) {
        next = KK_UI_FOCUS_CONFIRM;
    } else if (next > KK_UI_FOCUS_CONFIRM) {
        next = KK_UI_FOCUS_VALUE;
    }
    kk_ui_dialog_set_focus((uint8_t)next, now);
}

#else

void KK_UI_DialogInput(KK_UI_InputEvent event, uint32_t now)
{
    (void)event;
    (void)now;
}

#endif

static void kk_ui_toast_animate(uint32_t now)
{
#if KK_UI_ENABLE_TOAST
    if (kk_ui.toast.visible == 0U) {
        return;
    }
    if (kk_ui.toast.closing == 0U &&
        (int32_t)(now - kk_ui.toast.expires_at) >= 0) {
        kk_ui.toast.closing = 1U;
        kk_ui.toast.shown_at = now;
    }
    if (kk_ui.toast.closing != 0U) {
        uint16_t p = KK_UI_EaseQ12(now - kk_ui.toast.shown_at, KK_UI_TOAST_MS);
        kk_ui.toast.phase_q12 = (uint16_t)(KK_UI_Q12_ONE - p);
        kk_ui.dirty = 1U;
        if (p == KK_UI_Q12_ONE) {
            kk_ui.toast.visible = 0U;
        }
    } else if (kk_ui.toast.phase_q12 < KK_UI_Q12_ONE) {
        kk_ui.toast.phase_q12 = KK_UI_EaseQ12(now - kk_ui.toast.shown_at,
                                              KK_UI_TOAST_MS);
        kk_ui.dirty = 1U;
    }
#else
    (void)now;
#endif
}

void KK_UI_DialogAnimate(uint32_t now)
{
#if KK_UI_MODAL_ENABLED
    if (KK_UI_DialogActive()) {
        uint32_t elapsed = now - kk_ui.overlay.phase_started;
        bool redraw = false;
        if (kk_ui.overlay.closing != 0U) {
            uint16_t p = KK_UI_EaseQ12(elapsed, KK_UI_DIALOG_CLOSE_MS);
            kk_ui.overlay.phase_q12 = (uint16_t)KK_UI_LerpQ12(
                kk_ui.overlay.phase_from_q12, 0, p);
            redraw = true;
            if (p == KK_UI_Q12_ONE) {
                kk_ui.overlay.type = KK_UI_OVERLAY_NONE;
                kk_ui.overlay.closing = 0U;
            }
        } else if (kk_ui.overlay.phase_q12 < KK_UI_Q12_ONE) {
            kk_ui.overlay.phase_q12 = KK_UI_EaseQ12(elapsed,
                                                    KK_UI_DIALOG_OPEN_MS);
            redraw = true;
        }
        if (kk_ui.overlay.focus_moving != 0U) {
            redraw = true;
            if (now - kk_ui.overlay.focus_started >= KK_UI_FOCUS_MS) {
                kk_ui.overlay.focus_moving = 0U;
            }
        }
        if (kk_ui.overlay.value_moving != 0U) {
            uint32_t duration = kk_ui.overlay.type == KK_UI_OVERLAY_BOOL ?
                                KK_UI_BOOL_VALUE_MS : KK_UI_INT_VALUE_MS;
            redraw = true;
            kk_ui.overlay.value_q12 = KK_UI_EaseQ12(
                now - kk_ui.overlay.value_started, duration);
            if (kk_ui.overlay.value_q12 == KK_UI_Q12_ONE) {
                kk_ui.overlay.value_moving = 0U;
            }
        }
        if (redraw) {
            kk_ui.dirty = 1U;
        }
    }
#endif
    kk_ui_toast_animate(now);
}

KK_UI_Status KK_UI_ToastOpen(const char *text, uint32_t duration,
                             uint32_t now)
{
#if KK_UI_ENABLE_TOAST
    if (text == NULL) {
        return KK_UI_INVALID_ARGUMENT;
    }
    if (KK_UI_DialogActive()) {
        return KK_UI_BUSY;
    }
    if (duration == 0U) {
        duration = KK_UI_TOAST_DEFAULT_MS;
    }
    if (duration > (uint32_t)INT32_MAX) {
        duration = (uint32_t)INT32_MAX;
    }
    kk_ui.toast.text = text;
    kk_ui.toast.shown_at = now;
    kk_ui.toast.expires_at = now + duration;
    kk_ui.toast.phase_q12 = 0U;
    kk_ui.toast.visible = 1U;
    kk_ui.toast.closing = 0U;
    kk_ui.dirty = 1U;
    return KK_UI_OK;
#else
    (void)text; (void)duration; (void)now;
    return KK_UI_UNSUPPORTED;
#endif
}

#if KK_UI_MODAL_ENABLED
static const char *kk_ui_dialog_title(void)
{
    if (kk_ui.overlay.type == KK_UI_OVERLAY_INT) {
        return kk_ui.app->int_bindings[kk_ui.overlay.ref].title;
    }
    if (kk_ui.overlay.type == KK_UI_OVERLAY_BOOL) {
        return kk_ui.app->bool_bindings[kk_ui.overlay.ref].title;
    }
    if (kk_ui.overlay.type == KK_UI_OVERLAY_ENUM) {
        return kk_ui_enum_binding()->title;
    }
    return kk_ui.app->texts.message_title;
}

static void kk_ui_draw_dialog_text(const char *text)
{
    const char *newline;
    char first[32];
    size_t length = 0U;

    if (text == NULL) {
        return;
    }
    newline = text;
    while (*newline != '\0' && *newline != '\n') {
        ++newline;
    }
    if (*newline == '\n') {
        while (text[length] != '\n' && length + 1U < sizeof(first)) {
            first[length] = text[length];
            ++length;
        }
        first[length] = '\0';
        KK_UI_DrawCentered(10, 52, 108U, first);
        KK_UI_DrawCentered(10, 68, 108U, newline + 1);
    } else {
        KK_UI_DrawCentered(10, 61, 108U, text);
    }
}

static void kk_ui_draw_int_value(void)
{
    const KK_UI_IntBinding *binding =
        &kk_ui.app->int_bindings[kk_ui.overlay.ref];
    char old_text[20];
    char new_text[20];

    KK_UI_FormatInt(kk_ui.overlay.previous, binding->unit,
                    old_text, sizeof(old_text));
    KK_UI_FormatInt(kk_ui.overlay.draft, binding->unit,
                    new_text, sizeof(new_text));
    TFT_SetClipWindow(39, 68, 46U, 20U);
    if (kk_ui.overlay.value_moving != 0U) {
        int16_t offset = (int16_t)(16 * kk_ui.overlay.value_q12 /
                                   KK_UI_Q12_ONE);
        if (kk_ui.overlay.value_direction > 0) {
            KK_UI_DrawCentered(39, (int16_t)(69 - offset), 46U, old_text);
            KK_UI_DrawCentered(39, (int16_t)(85 - offset), 46U, new_text);
        } else {
            KK_UI_DrawCentered(39, (int16_t)(69 + offset), 46U, old_text);
            KK_UI_DrawCentered(39, (int16_t)(53 + offset), 46U, new_text);
        }
    } else {
        KK_UI_DrawCentered(39, 69, 46U, new_text);
    }
}

static void kk_ui_draw_bool_value(void)
{
    const KK_UI_BoolBinding *binding =
        &kk_ui.app->bool_bindings[kk_ui.overlay.ref];
    const char *on_text = binding->on_text != NULL ? binding->on_text :
                                                     kk_ui.app->texts.on_text;
    const char *off_text = binding->off_text != NULL ? binding->off_text :
                                                       kk_ui.app->texts.off_text;
    bool on = kk_ui.overlay.draft != 0;
    uint16_t p = kk_ui.overlay.value_moving != 0U ?
                 kk_ui.overlay.value_q12 : KK_UI_Q12_ONE;

    /*
     * 全局的开/关只占一个字宽，沿用原来的紧贴开关的位置；
     * 绑定自带文字（如「自动/手动」）时改用两侧整块留白居中，
     * 否则两个汉字会碰到开关图形。
     */
    if (binding->on_text != NULL || binding->off_text != NULL) {
        KK_UI_DrawCentered(15, 69, 32U, off_text);
        KK_UI_DrawCentered(82, 69, 32U, on_text);
    } else {
        KK_UI_DrawCentered(31, 69, 14U, off_text);
        KK_UI_DrawCentered(83, 69, 14U, on_text);
    }
    KK_UI_DrawSwitch(48, 70, 33U, 14U, on, p);
}

/* 单个选项框：方角矩形，与相邻行共用一条边；反白=待选项，四角短线=当前生效项 */
static void kk_ui_draw_enum_row(int16_t y, const char *label, bool inverted,
                                bool marked)
{
    const int16_t x = KK_UI_ENUM_BOX_LEFT;
    const uint16_t width = KK_UI_ENUM_BOX_WIDTH;
    const int16_t x1 = (int16_t)(x + (int16_t)width - 1);
    const int16_t y1 = (int16_t)(y + KK_UI_ENUM_ROW_HEIGHT - 1);

    /* 先画外框：反白行只在框内铺白，共用的那条边不会被抹掉 */
    TFT_DrawFrame(x, y, width, (uint16_t)KK_UI_ENUM_ROW_HEIGHT);

    if (inverted) {
        /* 反白：实心白块 + CLEAR 写字，与首页选中项同一套做法 */
        TFT_SetDrawMode(TFT_DRAW_SET);
        TFT_DrawBox((int16_t)(x + 1), (int16_t)(y + 1),
                    (uint16_t)(width - 2U),
                    (uint16_t)(KK_UI_ENUM_ROW_HEIGHT - 2));
        TFT_SetDrawMode(TFT_DRAW_CLEAR);
    } else {
        TFT_SetDrawMode(TFT_DRAW_SET);
    }

    if (marked) {
        /* 四个角各两段短线组成直角，内缩 3 像素落在框内侧 */
        const int16_t in = 3;
        const uint16_t len = 4U;

        TFT_DrawHLine((int16_t)(x + in), (int16_t)(y + in), len);
        TFT_DrawVLine((int16_t)(x + in), (int16_t)(y + in), len);
        TFT_DrawHLine((int16_t)(x1 - in - (int16_t)len + 1),
                      (int16_t)(y + in), len);
        TFT_DrawVLine((int16_t)(x1 - in), (int16_t)(y + in), len);
        TFT_DrawHLine((int16_t)(x + in), (int16_t)(y1 - in), len);
        TFT_DrawVLine((int16_t)(x + in),
                      (int16_t)(y1 - in - (int16_t)len + 1), len);
        TFT_DrawHLine((int16_t)(x1 - in - (int16_t)len + 1),
                      (int16_t)(y1 - in), len);
        TFT_DrawVLine((int16_t)(x1 - in),
                      (int16_t)(y1 - in - (int16_t)len + 1), len);
    }

    /* 文字画在框内第一行之后，正好占满 16 行，上下各留 1 像素边线 */
    KK_UI_DrawCentered(x, y, width, label);
    TFT_SetDrawMode(TFT_DRAW_SET);
}

static void kk_ui_draw_enum_value(void)
{
    const KK_UI_EnumBinding *binding = kk_ui_enum_binding();
    const uint16_t count = kk_ui_enum_count();
    const int16_t top = kk_ui_enum_top();
    uint16_t i;

    KK_UI_DrawCentered(10, (int16_t)(top + 2), 108U, binding->title);
    TFT_DrawHLine(12, (int16_t)(top + 19), 104U);

    for (i = 0U; i < count; ++i) {
        if (binding->options[i] == NULL) {
            continue;
        }
        kk_ui_draw_enum_row(kk_ui_enum_row_top(i), binding->options[i],
                            i == (uint16_t)kk_ui.overlay.draft,
                            i == (uint16_t)kk_ui.overlay.original);
    }

    /* 确定按钮：圆角框（与列表的方角区分）；焦点在它上面时反白 */
    if (kk_ui.overlay.focus == KK_UI_FOCUS_CONFIRM) {
        TFT_DrawRBox(KK_UI_ENUM_BOX_LEFT, kk_ui_enum_confirm_top(),
                     (uint16_t)KK_UI_ENUM_BOX_WIDTH,
                     (uint16_t)KK_UI_ENUM_CONFIRM_HEIGHT, 3U);
        TFT_SetDrawMode(TFT_DRAW_CLEAR);
        KK_UI_DrawCentered(KK_UI_ENUM_BOX_LEFT, kk_ui_enum_confirm_top(),
                           (uint16_t)KK_UI_ENUM_BOX_WIDTH,
                           kk_ui.app->texts.confirm_text);
        TFT_SetDrawMode(TFT_DRAW_SET);
    } else {
        TFT_DrawRFrame(KK_UI_ENUM_BOX_LEFT, kk_ui_enum_confirm_top(),
                       (uint16_t)KK_UI_ENUM_BOX_WIDTH,
                       (uint16_t)KK_UI_ENUM_CONFIRM_HEIGHT, 3U);
        KK_UI_DrawCentered(KK_UI_ENUM_BOX_LEFT, kk_ui_enum_confirm_top(),
                           (uint16_t)KK_UI_ENUM_BOX_WIDTH,
                           kk_ui.app->texts.confirm_text);
    }
}

void KK_UI_DialogDraw(void)
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int32_t focus_x;
    int32_t focus_y;
    int32_t focus_w;
    int32_t focus_h;
    uint16_t p;
    bool is_enum;

    if (!KK_UI_DialogActive()) {
        return;
    }
    is_enum = kk_ui.overlay.type == KK_UI_OVERLAY_ENUM;
    p = kk_ui.overlay.phase_q12;
    x = (int16_t)(3 + 3 * p / KK_UI_Q12_ONE);
    /* 多选一的高度随选项数变，展开终点随之变化。 */
    y = (int16_t)(kk_ui.overlay.origin_y +
                  ((int32_t)(is_enum ? kk_ui_enum_top() : KK_UI_DIALOG_TOP) -
                   kk_ui.overlay.origin_y) * p / KK_UI_Q12_ONE);
    width = (int16_t)(118 - 2 * p / KK_UI_Q12_ONE);
    height = (int16_t)(16 + ((is_enum ? (int32_t)kk_ui_enum_height() : 61) - 16) *
                       p / KK_UI_Q12_ONE);
    TFT_ResetClipWindow();
    TFT_SetDrawMode(TFT_DRAW_CLEAR);
    TFT_DrawRBox(x, y, (uint16_t)width, (uint16_t)height, 3U);
    TFT_SetDrawMode(TFT_DRAW_SET);
    TFT_DrawRFrame(x, y, (uint16_t)width, (uint16_t)height, 3U);
    if (width <= 2 || height <= 2) {
        return;
    }
    TFT_SetClipWindow((int16_t)(x + 1), (int16_t)(y + 1),
                       (uint16_t)(width - 2), (uint16_t)(height - 2));
    TFT_SetFont(kk_ui.app->fonts.body_font);
    if (is_enum) {
        kk_ui_draw_enum_value();
    } else if (kk_ui.overlay.type == KK_UI_OVERLAY_CONFIRM) {
        kk_ui_draw_dialog_text(kk_ui.overlay.text);
        KK_UI_DrawCentered(15, 89, 98U, kk_ui.app->texts.confirm_text);
    } else {
        KK_UI_DrawCentered(10, 51, 108U, kk_ui_dialog_title());
        TFT_DrawHLine(12, 67, 104U);
        if (kk_ui.overlay.type == KK_UI_OVERLAY_INT) {
            kk_ui_draw_int_value();
            TFT_SetClipWindow((int16_t)(x + 1), (int16_t)(y + 1),
                               (uint16_t)(width - 2),
                               (uint16_t)(height - 2));
            KK_UI_DrawCentered(15, 89, 98U, kk_ui.app->texts.confirm_text);
            if (kk_ui.overlay.editing != 0U) {
                const KK_UI_IntBinding *binding =
                    &kk_ui.app->int_bindings[kk_ui.overlay.ref];
                if (kk_ui.overlay.draft < binding->maximum) {
                    TFT_DrawLine(91, 74, 95, 70);
                    TFT_DrawLine(95, 70, 99, 74);
                }
                if (kk_ui.overlay.draft > binding->minimum) {
                    TFT_DrawLine(91, 79, 95, 83);
                    TFT_DrawLine(95, 83, 99, 79);
                }
            }
        } else if (kk_ui.overlay.type == KK_UI_OVERLAY_BOOL) {
            kk_ui_draw_bool_value();
            KK_UI_DrawCentered(15, 89, 98U, kk_ui.app->texts.confirm_text);
        } else {
            KK_UI_DrawCentered(8, 71, 112U, kk_ui.overlay.text);
            KK_UI_DrawCentered(46, 89, 36U, kk_ui.app->texts.confirm_text);
        }
    }
    /*
     * 多选一的焦点已经用「反白选项 / 反白确定按钮」表达，
     * 再叠一层 XOR 框会把反白抵消掉，所以只对它跳过。
     */
    if (!is_enum) {
        kk_ui_dialog_current_focus(kk_ui.last_update, &focus_x, &focus_y,
                                   &focus_w, &focus_h);
        TFT_SetDrawMode(TFT_DRAW_XOR);
        TFT_DrawRBox(KK_UI_RoundQ8(focus_x), KK_UI_RoundQ8(focus_y),
                      (uint16_t)KK_UI_RoundQ8(focus_w),
                      (uint16_t)KK_UI_RoundQ8(focus_h), 3U);
        TFT_SetDrawMode(TFT_DRAW_SET);
    }
    TFT_ResetClipWindow();
}

#else

void KK_UI_DialogDraw(void)
{
}

#endif

void KK_UI_ToastDraw(void)
{
#if KK_UI_ENABLE_TOAST
    uint16_t width;
    int16_t x;
    int16_t y;
    if (kk_ui.toast.visible == 0U || KK_UI_DialogActive()) {
        return;
    }
    TFT_SetFont(kk_ui.app->fonts.body_font);
    width = (uint16_t)(TFT_GetUTF8Width(kk_ui.toast.text) + 12U);
    if (width > 120U) {
        width = 120U;
    }
    if ((width & 1U) != 0U) {
        ++width;
    }
    x = (int16_t)((KK_UI_SCREEN_WIDTH - width) / 2);
    y = (int16_t)(KK_UI_SCREEN_HEIGHT -
                  (16 * kk_ui.toast.phase_q12 / KK_UI_Q12_ONE));
    TFT_SetClipWindow(x, y, width, 16U);

    /* Make the overlay independent of the page beneath it. Clearing the full
     * bounding box also leaves clean black corner pixels around the rounded
     * white label instead of allowing page glyphs to touch its silhouette. */
    TFT_SetDrawMode(TFT_DRAW_CLEAR);
    TFT_DrawBox(x, y, width, 16U);
    TFT_SetDrawMode(TFT_DRAW_SET);
    TFT_DrawRBox(x, y, width, 16U, 3U);

    /* Draw black text into the opaque white label. */
    TFT_SetDrawMode(TFT_DRAW_CLEAR);
    KK_UI_DrawCentered(x, (int16_t)(y + 1), width, kk_ui.toast.text);
    TFT_SetDrawMode(TFT_DRAW_SET);
    TFT_ResetClipWindow();
#endif
}

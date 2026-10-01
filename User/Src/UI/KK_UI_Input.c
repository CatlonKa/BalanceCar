#include "KK_UI_Input.h"

#if KK_UI_INPUT_HAS_WHEEL
#include "encoder.h"
#include "event.h"
#endif

/*
 * 唯一的硬件接口点：从左右车轮编码器取“自上次调用以来的计数增量”。
 *
 * 编码器同一时刻只能有一个读者，靠 car_run 把两个阶段错开（见 User/Src/event.c）：
 *
 *   car_run = 1（电机在转）→ 车轮归电机，TIM6 打开做测速，UI 完全不碰编码器；
 *   car_run = 0（已停机）  → 车轮归 UI，TIM6 关掉，UI 独占编码器读手势。
 *
 * 这样一来 encoder.c 的 encoder_get_delta_left/right() 那种“读一次就清零”
 * 的写法可以直接用，不需要改造，也不会有谁偷走谁的计数。
 *
 * 下面 300 ms 静默期是为了避开车轮惯性：用户按 KEY0 停机的那一瞬间车轮还在转，
 * 那段转动不是用户输入，必须读掉丢掉，不能算成手势。
 */

#if KK_UI_INPUT_HAS_WHEEL

/* 停机后到开始接受手势之间的静默期，用来避开车轮惯性转动。 */
#define KK_UI_INPUT_UNLOCK_BLANK_MS 300U

static bool s_wheel_usable;       /**< 上一轮的车轮归属，用于识别阶段切换。 */
static uint32_t s_wheel_ready_at; /**< 静默期结束时刻。 */

bool KK_UI_InputWheelUsable(void)
{
    /* car_run = 1 表示电机在转，车轮归电机，此时 UI 不采样也不响应任何操作。 */
    return car_run == 0;
}

static bool KK_UI_RawWheelSample(int16_t *left_delta, int16_t *right_delta,
                                 uint32_t now_ms)
{
    if (car_run) {
        if (s_wheel_usable) {
            /* 进入电机运行阶段：冻结手势并清掉 UI 自己那份累计量。 */
            s_wheel_usable = false;
            KK_UI_InputReset();
        }
        return false;
    }

    if (!s_wheel_usable) {
        /*
         * 刚停机（或上电）后的第一次可用：清一次 UI 累计量，
         * 把切换瞬间残留的计数读掉丢掉，再静默一段才开始接受手势。
         */
        s_wheel_usable = true;
        KK_UI_InputReset();
        (void)encoder_get_delta_left();
        (void)encoder_get_delta_right();
        s_wheel_ready_at = now_ms + KK_UI_INPUT_UNLOCK_BLANK_MS;
        return false;
    }

    if ((int32_t)(now_ms - s_wheel_ready_at) < 0) {
        /*
         * 静默期内：计数仍然要读掉丢掉。
         * 不读的话惯性转动会一路堆积，到静默期结束一次性冒出一串假手势。
         */
        (void)encoder_get_delta_left();
        (void)encoder_get_delta_right();
        return false;
    }

    /*
     * 符号约定：encoder_get_delta_left/right() 都以“车轮前进为正”返回
     * （右轮的取反已经在 encoder.c 里做好了），正好符合 KK_UI 的约定。
     */
    *left_delta = encoder_get_delta_left();
    *right_delta = encoder_get_delta_right();
    return true;
}

#else /* !KK_UI_INPUT_HAS_WHEEL */

bool KK_UI_InputWheelUsable(void)
{
    /*
     * 不接车轮时的退路：界面照常运行，方便单独上屏验证显示与布局；
     * 手势冻结（采样恒返回 false），所以界面不会被误触发。
     */
    return true;
}

static bool KK_UI_RawWheelSample(int16_t *left_delta, int16_t *right_delta,
                                 uint32_t now_ms)
{
    (void)left_delta;
    (void)right_delta;
    (void)now_ms;
    return false;
}

#endif /* KK_UI_INPUT_HAS_WHEEL */

/* KK_UI 核心用“稳定电平变化 + 去抖时间”识别确定键，所以确定键必须真的
 * 先按下、保持一段时间、再松开，才能产生一次按下事件。 */
#define KK_UI_INPUT_OK_HOLD_MS 40U

/*
 * 确定键的保持时间也是手势，必须短于两次手势的最小间隔，
 * 否则冷却时间还没走完确定键还没松开，下一次手势就已经放行了。
 */
#if KK_UI_INPUT_GESTURE_INTERVAL_MS <= KK_UI_INPUT_OK_HOLD_MS
#error "KK_UI_INPUT_GESTURE_INTERVAL_MS must be greater than KK_UI_INPUT_OK_HOLD_MS"
#endif

/*
 * 漏积分的参数合法性：SHIFT 为 0 表示每次把全部累计量削光，等同于禁用累计；
 * 太大则位移溢出 uint16_t 的除数。
 */
#if KK_UI_INPUT_DECAY_SHIFT < 1U || KK_UI_INPUT_DECAY_SHIFT > 14U
#error "KK_UI_INPUT_DECAY_SHIFT must be in 1..14"
#endif

#if KK_UI_INPUT_DECAY_PERIOD_MS < 1U
#error "KK_UI_INPUT_DECAY_PERIOD_MS must be at least 1"
#endif

static int16_t s_left_accumulator;  /**< 左轮未消化的计数增量。 */
static int16_t s_right_accumulator; /**< 右轮未消化的计数增量。 */
static uint32_t s_ok_release_at;    /**< 非 0 表示确定键保持到该时刻后释放。 */
static uint32_t s_gesture_ready_at; /**< 非 0 表示冷却到该时刻前不再出手势。 */
static uint32_t s_decay_at;         /**< 上一次漏积分削峰的时刻。 */
static bool s_back_requested;       /**< 已经识别到一次返回手势。 */

/**
 * 把一个累计量按比例往零削一步。
 *
 * 用指数衰减（按自身比例削）而不是固定速率，是为了与 KK_UI_INPUT_WHEEL_STEPS
 * 的绝对大小无关：阈值从 300 改成 5500 也不需要重调衰减参数。
 *
 * 必须先取绝对值再算步长：C 的整数除法朝零截断，直接用负数去除会得到负的步长
 * （-100 / 8 == -12），加到负数累计量上反而让它离零更远，变成「越削越大」，
 * 于是只有一个方向的抖动被抑制住，另一个方向完全失效。
 */
static int16_t kk_ui_input_decay_step(int16_t value, int32_t divisor)
{
    int32_t magnitude; /* 累计量的绝对值，后续都在正数上运算。 */
    int32_t step;

    if (value == 0) {
        return 0;
    }
    magnitude = (value < 0) ? -(int32_t)value : (int32_t)value;
    step = magnitude / divisor;
    if (step == 0) {
        /* 数值很小的时候整数除法会算成 0，那样永远削不到底，所以至少削 1。 */
        step = 1;
    }
    magnitude -= step;
    if (magnitude < 0) {
        magnitude = 0;
    }
    return (value < 0) ? (int16_t)(-magnitude) : (int16_t)magnitude;
}

/**
 * 按经过的时间给两个累计量做漏积分。
 *
 * 以毫秒而不是调用次数计时：主循环节奏会变（界面忙的时候调用间隔不一样），
 * 按次数计的话削峰速度会跟着主循环负载漂移。
 */
static void kk_ui_input_decay(uint32_t now_ms)
{
    /* 补偿周期数的上限：循环被长时间拖住（例如上电初始化）时没必要真的补几百次。 */
    const uint32_t max_periods = 64U;
    const int32_t divisor = (int32_t)1 << KK_UI_INPUT_DECAY_SHIFT;
    uint32_t elapsed;
    uint32_t periods;
    uint32_t i;

    if (s_decay_at == 0U) {
        /* 第一次调用（或刚复位）：只记下起点，本轮不削。 */
        s_decay_at = now_ms;
        return;
    }
    elapsed = now_ms - s_decay_at;
    if (elapsed < KK_UI_INPUT_DECAY_PERIOD_MS) {
        return;
    }
    periods = elapsed / KK_UI_INPUT_DECAY_PERIOD_MS;
    /* 只推进整数个周期，余下的零头留到下一轮，避免主循环变快时削峰也变快。 */
    s_decay_at += periods * KK_UI_INPUT_DECAY_PERIOD_MS;
    if (periods > max_periods) {
        periods = max_periods;
    }

    for (i = 0U; i < periods; ++i) {
        s_left_accumulator = kk_ui_input_decay_step(s_left_accumulator, divisor);
        s_right_accumulator = kk_ui_input_decay_step(s_right_accumulator, divisor);
    }
}

void KK_UI_InputReset(void)
{
    s_left_accumulator = 0;
    s_right_accumulator = 0;
    s_ok_release_at = 0U;
    s_gesture_ready_at = 0U;
    s_decay_at = 0U;
    s_back_requested = false;
}

bool KK_UI_InputTakeBackRequest(void)
{
    bool requested = s_back_requested;
    s_back_requested = false;
    return requested;
}

KK_UI_Input KK_UI_InputRead(uint32_t now_ms)
{
    KK_UI_Input input;
    int16_t left_delta = 0;
    int16_t right_delta = 0;
    bool cooling;

    input.keys = 0U;
    input.encoder_delta = 0;

    if (KK_UI_RawWheelSample(&left_delta, &right_delta, now_ms)) {
        s_left_accumulator += left_delta;
        s_right_accumulator += right_delta;
    }

    /*
     * 漏积分要在判断阈值之前做：这样「转得慢」的输入会先被削掉一部分，
     * 涨不到阈值；只有转得足够快、补充速度超过削峰速度时才可能触发。
     */
    kk_ui_input_decay(now_ms);

    /*
     * 冷却期：上一次手势之后 KK_UI_INPUT_GESTURE_INTERVAL_MS 内不再产生新手势。
     *
     * 这段转动的计数照样读走，但立即丢弃：用户拨一下车轮之后轮子会因为
     * 惯性继续转，不丢的话冷却一结束那些残留计数会马上凑出第二次手势，
     * 表现成“拨一格跳两格”。
     */
    cooling = s_gesture_ready_at != 0U &&
              (int32_t)(now_ms - s_gesture_ready_at) < 0;
    if (cooling) {
        s_left_accumulator = 0;
        s_right_accumulator = 0;
    }

    /*
     * 右轮：前进 = 上一个，后退 = 下一个。
     * KK_UI 约定 encoder_delta 为正表示“向下”，所以前进对应负增量。
     * 用编码器增量表达上/下不经过按键去抖，手感更接近旋钮。
     */
    if (!cooling) {
        if (s_right_accumulator >= KK_UI_INPUT_WHEEL_STEPS) {
            input.encoder_delta = -1;
            s_right_accumulator -= KK_UI_INPUT_WHEEL_STEPS;
            s_gesture_ready_at = now_ms + KK_UI_INPUT_GESTURE_INTERVAL_MS;
        } else if (s_right_accumulator <= -KK_UI_INPUT_WHEEL_STEPS) {
            input.encoder_delta = 1;
            s_right_accumulator += KK_UI_INPUT_WHEEL_STEPS;
            s_gesture_ready_at = now_ms + KK_UI_INPUT_GESTURE_INTERVAL_MS;
        }
    }

    /*
     * 左轮：前进 = 确定，后退 = 取消（交给 KK_UI_RequestBack()）。
     *
     * 确定键这一路走按键去抖，需要“按下并保持”再松开，所以在冷却期内
     * 仍然必须继续往外发，否则 40 ms 的保持会被冷却时间吃掉。
     */
    if (s_ok_release_at == 0U && !cooling) {
        if (s_left_accumulator >= KK_UI_INPUT_WHEEL_STEPS) {
            s_left_accumulator -= KK_UI_INPUT_WHEEL_STEPS;
            s_ok_release_at = now_ms + KK_UI_INPUT_OK_HOLD_MS;
            s_gesture_ready_at = now_ms + KK_UI_INPUT_GESTURE_INTERVAL_MS;
        } else if (s_left_accumulator <= -KK_UI_INPUT_WHEEL_STEPS) {
            s_left_accumulator += KK_UI_INPUT_WHEEL_STEPS;
            s_back_requested = true;
            s_gesture_ready_at = now_ms + KK_UI_INPUT_GESTURE_INTERVAL_MS;
        }
    }

    if (s_ok_release_at != 0U) {
        if ((int32_t)(now_ms - s_ok_release_at) < 0) {
            input.keys |= KK_UI_KEY_OK;
        } else {
            s_ok_release_at = 0U;
        }
    }

    return input;
}

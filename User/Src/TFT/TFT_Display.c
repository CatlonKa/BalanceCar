#include "TFT_Display.h"

#include "KK_UI_App.h"
#include "TFT.h"
#include "TFT_BootSplash.h"
#include "event.h"

/** 屏幕是否已经初始化，并且仍处于供电状态。 */
static bool s_screen_ready = false;

/** 开机图停留到什么时候（HAL_GetTick 时刻）。0 表示没有在停留。 */
static uint32_t s_splash_until = 0U;

void TFT_DisplayInit(void)
{
    /*
     * 上电刻意不初始化屏幕。
     *
     * 默认状态是「电机自己在转」（car_run = 1，见 event.c），而一动电机屏幕就会
     * 被拔掉/断电，所以现在点亮只是白花 TFT_Init() 那几百毫秒，还会闪一下。
     * 屏幕初始化推迟到第一次按 KEY0 停车时，由 TFT_DisplaySync() 完成。
     *
     * TFT_DeInit() 会把本地状态置成未就绪、关掉背光，并让面板进睡眠。
     * 最后一步是关键：上电时定时器刚复位，PB1 的输出状态不确定，不管它背光
     * 可能一直亮；而面板睡下去之后，屏幕即使一直插着也是黑的，看起来就是运动状态。
     */
    TFT_DeInit();
    s_screen_ready = false;
    s_splash_until = 0U;
}

bool TFT_DisplaySync(void)
{
    /*
     * 开机图停留到期 -> 这时才正式开放 UI。
     *
     * 停留不在这里死等：本函数每轮都被主循环调用，记下结束时刻、每轮比一下
     * 就够了。这样停留期间主循环照常跑（电压采样、串口都在工作），
     * 屏幕亮着与系统推进互不阻塞。
     *
     * 时刻比较写成有符号差，所以 HAL_GetTick() 回绕时依然正确。
     */
    if (s_splash_until != 0U && (int32_t)(HAL_GetTick() - s_splash_until) >= 0)
    {
        s_splash_until = 0U;
        s_screen_ready = true;
    }

    /*
     * 车轮归属由 KEY0 在 TIM5 中断里翻转，这里负责收尾。
     * 屏幕初始化有几百毫秒阻塞延时，绝不能放进中断，所以只能由主循环驱动。
     */
    if (!car_state_changed)
    {
        return s_screen_ready;
    }
    car_state_changed = 0;

    if (car_run == 0)
    {
        /*
         * 停车：车轮交回 UI，顺便把刚上电的屏幕初始化好。
         *
         * 屏幕每次都是冷启动（动电机时会断电），所以这里完整重走一遍 ST7735S
         * 的初始化序列，不做任何「已经初始化过就跳过」的增量判断。
         */
        if (TFT_Init() != TFT_OK)
        {
            /* 初始化失败就不开放 UI：宁可黑屏，也不要拿半初始化状态去画界面。 */
            s_screen_ready = false;
            return s_screen_ready;
        }

        /*
         * 屏幕刚点亮，先放彩色开机图。每次取下一张，依次轮换。
         *
         * 结果刻意不参与可用性判断：开机图只是锦上添花，画不出来
         * 也不该让整个界面不可用。
         */
        (void)TFT_BootSplashShow(TFT_BootSplashPickIndex());

        /* 界面初始化失败同样不开放 UI，下一次停车还会再试一遍。 */
        if (KK_UI_AppInit() != KK_UI_OK)
        {
            s_screen_ready = false;
            return s_screen_ready;
        }

        /*
         * 界面已经就绪，但先让开机图留在屏幕上 TFT_BOOT_SPLASH_HOLD_MS，
         * 这段时间不推进 UI（s_screen_ready 仍是 false）。下一次调用本函数时
         * 上面第一段会把它转成 true。
         */
        s_splash_until = HAL_GetTick() + TFT_BOOT_SPLASH_HOLD_MS;
        s_screen_ready = false;
    }
    else
    {
        /*
         * 起步：屏幕马上要被拔掉/断电。
         *
         * 这里不是只关背光：TFT_DeInit() 还会让面板关显示并进睡眠。
         * 所以就算用户这次没拔屏幕，它也是全黑的 —— 只看屏幕亮不亮就能分清
         * 现在是运动状态还是 UI 状态。
         *
         * 停留期间（s_screen_ready 还是 false）也必须走这一步，否则屏幕会亮着。
         */
        if (s_screen_ready || s_splash_until != 0U)
        {
            TFT_DeInit();
        }
        s_screen_ready = false;
        s_splash_until = 0U;
    }

    return s_screen_ready;
}

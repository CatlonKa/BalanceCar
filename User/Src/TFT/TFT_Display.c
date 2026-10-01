#include "TFT_Display.h"

#include "KK_UI_App.h"
#include "TFT.h"
#include "event.h"

/** 屏幕是否已经初始化，并且仍处于供电状态。 */
static bool s_screen_ready = false;

void TFT_DisplayInit(void)
{
    /*
     * 上电刻意不初始化屏幕。
     *
     * 默认状态是「电机自己在转」（car_run = 1，见 event.c），而一动电机屏幕就会
     * 被拔掉/断电，所以现在点亮只是白花 TFT_Init() 那五百多毫秒，还会闪一下。
     * 屏幕初始化推迟到第一次按 KEY0 停车时，由 TFT_DisplaySync() 完成。
     *
     * 这里调一次 TFT_DeInit() 只是为了把本地状态置成未就绪，并确保背光 PWM 不启动：
     * 没有它的话 PB1 的输出取决于定时器的复位状态，上电背光可能一直亮着。
     */
    TFT_DeInit();
    s_screen_ready = false;
}

bool TFT_DisplaySync(void)
{
    /*
     * 车轮归属由 KEY0 在 TIM5 中断里翻转，这里负责收尾。
     * 屏幕初始化有 500 ms 级阻塞延时，绝不能放进中断，所以只能由主循环驱动。
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
         *
         * 短路求值顺手保证了 TFT 没起来就不会去初始化 UI。任何一步失败都不开放
         * UI：宁可黑屏，也不要拿半初始化状态去画界面；下一次停车还会再试一遍。
         */
        s_screen_ready = (TFT_Init() == TFT_OK) && (KK_UI_AppInit() == KK_UI_OK);
    }
    else if (s_screen_ready)
    {
        /* 起步：屏幕马上要被拔掉/断电，关背光并丢掉本地显示状态。 */
        TFT_DeInit();
        s_screen_ready = false;
    }

    return s_screen_ready;
}

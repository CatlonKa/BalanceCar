#ifndef TFT_BOOTSPLASH_H
#define TFT_BOOTSPLASH_H

#include "TFT.h"
#include "TFT_Config.h"
#include "TFT_BootSplashData.h"

#include <stdint.h>

/*
 * 彩色开机定格图。
 *
 * 屏幕初始化好之后、界面出现之前显示的一张静态图。它是全屏的，尺寸必须与
 * 面板可见区一致，所以下面用编译期断言把这个前提钉死。
 *
 * 图像数据是 RGB565（行优先、每像素两字节、高字节在前），直接对应面板线上
 * 字节顺序，因此可以整块从 Flash 发给 SPI，既不占 RAM 也不需要色彩转换。
 * 这也意味着它是**彩色**的：走 TFT_DirectBlit() 绕过 1bpp 帧缓冲，
 * 与界面那套只有黑白两色的显存无关。
 *
 * 数据由 TFT_Resources/images/lvgl_to_splash.py 生成：把 LVGL 导出的 .c
 * （里面的原始像素是小端序）翻成面板要的大端序，并生成图像表。换图请重跑脚本。
 *
 * 一共 TFT_BOOT_SPLASH_COUNT 张，每初始化一次屏幕显示其中一张，依次轮换。
 */

/**
 * 编译期自检：图像数据的字节数必须与面板可见区一致。
 *
 * 数据里的长度是生成脚本按它自己的尺寸常量算出来的，脚本不知道面板配置。
 * 所以改过 TFT_Config.h 之后如果忘了重新生成数据，就靠这条断言暴露；
 * 否则会按错误的宽高把图推上屏，现象是画面斜切或错位，很难定位。
 *
 * 注意这里是预处理器判断，只能写整型常量，不能加 (uint32_t) 之类的强制转换
 * ——预处理阶段还没有类型。
 */
#if (TFT_BOOT_SPLASH_BYTES != (TFT_PHYSICAL_WIDTH * TFT_PHYSICAL_HEIGHT * 2))
#error "开机图数据尺寸与面板可见区不符：请重新运行 lvgl_to_splash.py 转换图片"
#endif

/**
 * 开机图停留时间（毫秒）。0 表示画完立刻进界面。
 *
 * ⚠️ 这个时间**不由本模块死等**，而是由调用方（TFT_Display.c）用主循环
 * 每轮比一次时刻来实现的。原因：屏幕每次都是冷启动（动电机时会断电），
 * 所以停留发生在**每次按 KEY0 停车**时；若在这里 HAL_Delay() 死等，
 * 主循环就被堵住，电压采样和串口都要停。
 *
 * 800 ms 是用户定的：比原先的 500 ms 长，而初始化侧同时压掉了约 150 ms，
 * 所以停车总体等待并没有变长。想更干脆就调小或设 0。
 */
#ifndef TFT_BOOT_SPLASH_HOLD_MS
#define TFT_BOOT_SPLASH_HOLD_MS 800U
#endif

/**
 * 把第 index 张开机图画上屏，阻塞约 16 ms（一次整屏 SPI 传输），**不停留**。
 *
 * 只在屏幕刚初始化好、还没有任何界面刷新在跑的时候调用（也就是
 * TFT_DisplaySync() 里 TFT_Init() 成功之后、KK_UI_AppInit() 之前）。
 * 它走的是 TFT_DirectBlit()，会作废内部「最近稳定帧」，使界面第一帧强制整屏。
 *
 * 停留由调用方计时（见 TFT_BOOT_SPLASH_HOLD_MS）。index 越界时按 0 处理。
 *
 * 返回值是 TFT_DirectBlit() 的结果。失败也不影响后续可用性，只是没有开机图，
 * 所以调用方通常不必处理。
 */
TFT_Status TFT_BootSplashShow(uint8_t index);

/**
 * 挑出本次要显示的那一张，并把内部游标推进到下一张，
 * 返回 0 ~ TFT_BOOT_SPLASH_COUNT-1。
 *
 * ⚠️ 游标目前只存在 RAM 里，所以「每次**上电**都换一张」还没真正成立：
 * 掉电后游标归零，又会从第 0 张开始。现在实际效果是
 * 「每初始化一次屏幕（也就是每次按 KEY0 停车）换一张」。
 *
 * 要做到跨掉电轮换，只需把实现里那两个 splash_index_load/save 改成
 * 读写非易失存储，其余逻辑不用动，见 .c 里的说明。
 */
uint8_t TFT_BootSplashPickIndex(void);

#endif /* TFT_BOOTSPLASH_H */

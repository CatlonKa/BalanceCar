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

/* 开机图停留时间（ms）。0 = 画完立刻进界面。
 * 不由本模块死等，由 TFT_Display.c 在主循环里比时刻实现（死等会堵住采样与串口）。 */
#ifndef TFT_BOOT_SPLASH_HOLD_MS
#define TFT_BOOT_SPLASH_HOLD_MS 800U
#endif

/* 显示第 index 张开机图，阻塞约 16 ms（一次整屏 SPI 传输），不停留；越界按 0。
 * 只应在屏幕刚初始化好、无刷新在跑时调用（TFT_DisplaySync 里 TFT_Init 之后）。
 * 走 TFT_DirectBlit()，会作废「最近稳定帧」-> 界面第一帧强制整屏。 */
TFT_Status TFT_BootSplashShow(uint8_t index);

/* 返回本次要显示的图下标并推进游标（0 ~ COUNT-1）。
 * 游标只在 RAM，掉电归零 -> 实际是「每初始化一次屏幕换一张」，不是「每次上电换一张」。 */
uint8_t TFT_BootSplashPickIndex(void);

#endif /* TFT_BOOTSPLASH_H */

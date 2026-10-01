#ifndef TFT_BOOTSPLASHDATA_H
#define TFT_BOOTSPLASHDATA_H

#include <stdint.h>

/*
 * 开机图数据表。由 TFT_Resources/images/lvgl_to_splash.py 生成，不要手改。
 *
 * 每张图的字节数与尺寸都由生成脚本按面板可见区核对过（对不上会直接报错），
 * 所以这里不再重复定义尺寸，只声明数组和数量。
 */

/** 开机图张数。 */
#define TFT_BOOT_SPLASH_COUNT 3U

/** 单张开机图的字节数。 */
#define TFT_BOOT_SPLASH_BYTES (128U * 160U * 2U)

/** 第 0 张开机图的 RGB565 字节流。 */
extern const uint8_t tft_boot_splash_0[TFT_BOOT_SPLASH_BYTES];

/** 第 1 张开机图的 RGB565 字节流。 */
extern const uint8_t tft_boot_splash_1[TFT_BOOT_SPLASH_BYTES];

/** 第 2 张开机图的 RGB565 字节流。 */
extern const uint8_t tft_boot_splash_2[TFT_BOOT_SPLASH_BYTES];

/** 图像指针表，长度就是 TFT_BOOT_SPLASH_COUNT。 */
extern const uint8_t *const tft_boot_splash_table[TFT_BOOT_SPLASH_COUNT];

#endif /* TFT_BOOTSPLASHDATA_H */

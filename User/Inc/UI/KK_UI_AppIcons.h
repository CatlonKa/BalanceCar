#ifndef KK_UI_APPICONS_H
#define KK_UI_APPICONS_H

#include <stdint.h>

/*
 * KK_UI 首页图标，32 x 32、LSB 优先的 XBM，与 KK_UI_HomeItem.icon_xbm_32x32
 * 以及 TFT_DrawXBM() 的位序一致。
 * 每行 4 字节，共 128 字节。这里只放当前最小样例实际用到的三个图标。
 */
extern const uint8_t kk_ui_icon_menu[128];
extern const uint8_t kk_ui_icon_status[128];
extern const uint8_t kk_ui_icon_wave[128];

#endif /* KK_UI_APPICONS_H */

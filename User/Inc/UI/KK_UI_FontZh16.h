#ifndef KK_UI_FONT_ZH16_H
#define KK_UI_FONT_ZH16_H

#include <stdint.h>

/*
 * 16 像素中文 + ASCII 字模，来自 LEDFont 在线取模助手的 wenquanyi_12pt（文泉驿点阵字 16）。
 * 字符集为可打印 ASCII（0x20～0x7E，95 个）加 KK_UI 界面用到的 32 个中文字，
 * 共 127 个字形，数组首个字节即字形数量。
 * 数据格式为 u8g2 压缩字体，由 TFT_Font.c 解析。
 *
 * 汉字集合（32 个）：
 *   亮关单压取器回定度开形态提波消状电确示程置菜蜂角返速里重限鸣图片
 * 其中「里」「程」当前界面没有使用，保留备后续做里程显示；
 * 「图」「片」用于首页的图片页入口标签。
 *
 * 重新取模：把新字加进 TFT_Resources/fonts/ui_charset.txt，再跑
 *   powershell -ExecutionPolicy Bypass -File TFT_Resources\fonts\gen_font.ps1
 * 该脚本会重写本文件、KK_UI_FontZh16.c 和归档副本，并自动把下面的数组长度
 * 改成服务端声明的值，所以不需要手工改数字。不要手工增删数组内容。
 * 本文件与 TFT_Font.c 里的解码器共用一条约定：字形数量由编译器填入
 * 「空字符串字面量」的第 0 字节，所以数组声明必须显式给出长度、不能省略。
 * 字体自身许可证由在线服务约束，不随本工程转为 MIT。
 *
 * 数组末尾的 0 是字符串字面量终止符，同时被字体解析器当作索引表结束标志，
 * 不要删除、不要在它后面追加数据。
 */
extern const uint8_t kk_ui_font_zh16[2677];

#endif /* KK_UI_FONT_ZH16_H */
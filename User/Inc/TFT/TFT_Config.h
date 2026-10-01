#ifndef TFT_CONFIG_H
#define TFT_CONFIG_H

/*
 * 本文件集中存放当前这块 LCD 的硬件事实。
 * 这里每个值都必须来自工程接线、模组资料或实测，不能照抄其他模组的常见值。
 */

#include "main.h"
#include "spi.h"

#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------- 面板 ---- */

/** 面板可见区物理宽度（像素）。 */
#define TFT_PHYSICAL_WIDTH 128U

/** 面板可见区物理高度（像素），必须是 8 的倍数：图形核心按每页 8 行组织显存。 */
#define TFT_PHYSICAL_HEIGHT 160U

/** 页式显存页数，每页 8 行。 */
#define TFT_PHYSICAL_PAGES (TFT_PHYSICAL_HEIGHT / 8U)

/*
 * ST7735S 内部显存为 132 x 162，128 x 160 只是它的一块可见子窗口，
 * 这两个偏移决定可见区左上角落在控制器显存中的哪个位置。
 *
 * (0, 0) 已在本模组上实物验证：贴满可见区的 1 像素边框四条边都完整压住
 * 屏幕边缘，方向正确。换模组时需要重新验证。
 *
 * 重新验证的办法：画一圈 TFT_DrawFrame(0, 0, TFT_GetWidth(), TFT_GetHeight())、
 * 四个角各加一块实心标记，四边都应贴住屏幕边缘；若一侧边框被裁掉、
 * 对侧出现黑边，就把黑边那一侧的偏移增大（边框被裁一侧的偏移减小）。
 */
#define TFT_GRAM_COLUMN_OFFSET 0U
#define TFT_GRAM_ROW_OFFSET    0U

/*
 * 0x36 Memory Access Control：bit7 MY、bit6 MX、bit5 MV、bit3 BGR。
 *
 * 0xC0 = MY | MX（不带 BGR）。
 *
 * ⚠️ bit3 是 2026-10-02 才真正定下的，别再照旧注释里的“已验证”去改：
 * 在那之前屏幕只有黑白两色（TFT_COLOR_FOREGROUND/BACKGROUND），而黑白下
 * R=G=B，BGR 位**完全没有可见影响**，所以当时根本验不出来。
 * 第一次上彩色图（开机图）就暴露了：红色整体偏蓝，正是 R/B 互换的典型症状。
 *
 * bit3 = 1 表示“先 R 后 B”，0 表示“先 B 后 R”。若以后换模组后发现
 * 红蓝又反了，改这一位即可（0xC0 <-> 0xC8）。
 *
 * MY 与 MX 同时置位等价于整体旋转 180°，属于纯旋转，用 TFT_SetRotation()
 * 就能纠回正方向，与本宏无关。
 */
#define TFT_MADCTL_VALUE 0xC0U

/** 0x3A Pixel Format Set 的 RGB565 编码。 */
#define TFT_COLMOD_VALUE 0x05U

/* ---------------------------------------------------------------- 颜色 ---- */

/**
 * 图形核心是 1-bit 页式显存，只有“亮/灭”两种取值，
 * 因此这里把两种取值各自映射成一个 RGB565 颜色。
 */
#define TFT_COLOR_FOREGROUND 0xFFFFU /**< 帧缓冲中为 1 的像素，白色。 */
#define TFT_COLOR_BACKGROUND 0x0000U /**< 帧缓冲中为 0 的像素，黑色。 */

/* ------------------------------------------------------------ 总线与引脚 ---- */

/** 面板使用的 SPI 句柄。 */
#define TFT_SPI_HANDLE hspi1

/** 单次阻塞 SPI 传输的最长等待时间（毫秒）。 */
#define TFT_BLOCKING_TIMEOUT_MS 100U

/*
 * 背光：PB1 接在 TIM8_CH3N 上做 PWM 调光，不再是普通 GPIO。
 *
 * TFT_BACKLIGHT_ACTIVE_HIGH 表示「引脚高电平是否对应点亮」：
 *   1 = 引脚高电平点亮（N-MOS 低边开关或直连背光）
 *   0 = 引脚低电平点亮（P-MOS 高边开关）
 * 实物上如果发现亮度反了（设 0 最亮、设 100 最暗），改这个宏即可。
 */
#define TFT_BACKLIGHT_ACTIVE_HIGH 1

/*
 * 亮度量程上限 TFT_BACKLIGHT_LEVEL_MAX 定义在 TFT.h（当前 100）：它和
 * TFT_SetBacklightLevel() 的入参范围属于对外 API 约定，所以不在本文件重复定义。
 *
 * 这里只负责硬件侧的对齐：TIM8 的 Period 必须等于该值（100−1），驱动才能把
 * 1 级亮度换算成 1% 占空比。用 CubeMX 改 TIM8 周期时，两处的数值要一起改。
 */

/*
 * CS / DC / RST 的引脚与端口宏由 CubeMX 生成在 main.h 中，
 * 分别是 TFT_CS_Pin、TFT_DC_Pin、TFT_RST_Pin 及其 _GPIO_Port。
 * BLK 已经改成 TIM8_CH3N，由 tim.h 的 htim8 驱动，没有 GPIO 宏。
 */

/* ------------------------------------------------------------ 帧保留策略 ---- */

/*
 * 帧提交后是否保留最近一帧的内容。
 *
 * 1 = 提交后把最新稳定帧镜像回绘制缓冲区。这是局部重绘的前提：上层可以
 *     TFT_BeginFrame(false) 只修改画面的一小块，其余像素沿用上一帧，
 *     再用 TFT_InvalidateRegion() 声明改动区，提交时只发送覆盖该区域的页。
 *     代价是每次提交多一次 2.5 KB 的 memcpy 和一次「稳定帧是否已就绪」的查询。
 * 0 = 提交后清空绘制缓冲区（本库原始行为）：每帧都必须完整重画整屏。
 *
 * 置 0 只是放弃局部重绘的收益，不会画错：TFT_BeginFrame(false) 会退回整屏重绘。
 */
#ifndef TFT_RETAIN_FRAME
#define TFT_RETAIN_FRAME 1
#endif

/* ------------------------------------------------------------ 编译期自检 ---- */

#if (TFT_PHYSICAL_HEIGHT % 8U) != 0U
#error "TFT_PHYSICAL_HEIGHT 必须是 8 的倍数，图形核心按每页 8 行组织显存"
#endif

#if TFT_PHYSICAL_WIDTH > 255U
#error "图形核心的列索引类型为 uint8_t，物理宽度不得超过 255"
#endif

#if TFT_PHYSICAL_PAGES > 255U
#error "图形核心的页索引类型为 uint8_t，页数不得超过 255"
#endif

#if (TFT_PHYSICAL_WIDTH * TFT_PHYSICAL_PAGES) > 65535U
#error "单帧字节数超出图形核心的 uint16_t 缓冲区索引上限"
#endif

#endif /* TFT_CONFIG_H */

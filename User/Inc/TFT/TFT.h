#ifndef TFT_H
#define TFT_H

/*
 * TFT 对外的唯一头文件。
 *
 * 应用层和业务模块只要 include 本文件就能使用全部公共 API。其余头文件都是
 * 实现细节，不要在 TFT/ 与 UI/ 目录之外 include：
 *
 *   TFT_Config.h    硬件事实：面板尺寸、GRAM 偏移、颜色、总线句柄、引脚约定
 *   TFT_Driver.h    驱动层（ST7735S + SPI1）接口，供 TFT_Driver.c / TFT_App.c
 *   TFT_Internal.h  图形层内部接口，供 TFT.c / TFT_Draw.c / TFT_Bitmap.c /
 *                   TFT_Font.c
 *
 * 依赖方向自下而上：TFT_Config -> TFT_Driver -> TFT_Internal -> TFT。
 * 本文件不 include 上面任何一个，保证外部代码拿到的依赖最小。
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** TFT 操作结果。异步刷新启动后请通过 TFT_GetLastStatus() 查询最终结果。 */
typedef enum {
    TFT_OK = 0,       /**< 操作成功。 */
    TFT_BUSY,         /**< 驱动正忙，本次请求未执行。 */
    TFT_ERROR,        /**< 初始化、启动或传输失败。 */
    TFT_UNSUPPORTED   /**< 当前驱动不支持该能力。 */
} TFT_Status;

/** 画布或文字方向，角度均为顺时针。 */
typedef enum {
    TFT_ROTATION_0 = 0, /**< 不旋转。 */
    TFT_ROTATION_90,    /**< 顺时针旋转 90°。 */
    TFT_ROTATION_180,   /**< 顺时针旋转 180°。 */
    TFT_ROTATION_270    /**< 顺时针旋转 270°。 */
} TFT_Rotation;

/** 图元前景像素如何作用于绘制缓冲区。 */
typedef enum {
    TFT_DRAW_CLEAR = 0, /**< 前景像素清零。 */
    TFT_DRAW_SET,       /**< 前景像素置一。 */
    TFT_DRAW_XOR        /**< 图元覆盖的每个像素各与原像素异或一次。 */
} TFT_DrawMode;

/** 文字和位图中值为 0 的背景像素是否写入。 */
typedef enum {
    TFT_BG_TRANSPARENT = 0, /**< 忽略文字或位图中的背景像素。 */
    TFT_BG_SOLID             /**< 用前景模式的反色写入背景像素。 */
} TFT_BackgroundMode;

/** 传入文字坐标的垂直参考语义。 */
typedef enum {
    TFT_FONT_POS_BASELINE = 0, /**< y 坐标表示文字基线。 */
    TFT_FONT_POS_TOP,          /**< y 坐标表示参考高度顶部。 */
    TFT_FONT_POS_BOTTOM,       /**< y 坐标表示参考高度底部。 */
    TFT_FONT_POS_CENTER        /**< y 坐标表示参考高度中心。 */
} TFT_FontPosition;

/** 计算文字参考高度时使用的 u8g2 兼容范围。 */
typedef enum {
    TFT_FONT_REF_TEXT = 0, /**< 使用 A 与 g 的常规文字高度。 */
    TFT_FONT_REF_EXTENDED, /**< 额外考虑括号等扩展文字高度。 */
    TFT_FONT_REF_ALL       /**< 使用字体中所有字形的最大高度。 */
} TFT_FontRefHeight;

/** 初始化图形状态和屏幕，清空显存后点亮显示。 */
TFT_Status TFT_Init(void);

/**
 * 关闭屏幕并释放显示层，本地图形状态全部作废。
 *
 * 除了清状态、停背光，还会让面板关显示并进睡眠（DISPOFF + SLPIN）。
 * 所以屏幕即使一直插着也会明确黑掉 —— 不用拔线就能一眼分清当前是运动状态
 * 还是 UI 状态。对已经被拔掉/断电的屏幕发命令同样安全：SPI 是主机驱动，
 * 从机不在也不影响主机完成传输，不会因此失败。
 *
 * 与 TFT_Init() 不同，本函数不检查“是否正忙”，也不清局部重绘状态之外的任何
 * 调用方数据。它表达的是“屏幕不再由我们驱动”，此时强行复位本地状态才是正确行为。
 * 调用后可以无条件重新 TFT_Init()。
 *
 * 没有返回值：关屏和释放本地状态都不会失败（内部实现见 TFT_DriverDeInit()）。
 */
void TFT_DeInit(void);

/** 阻塞刷新当前绘制帧，返回时提交已经完成。 */
TFT_Status TFT_Update(void);

/**
 * 把一块 RGB565 图像直接写进面板，绕过 1bpp 帧缓冲。
 *
 * 用于开机图、摄像头这类「不由图形核心生成」的图像：数据是行优先、
 * 每像素两字节、高字节在前的原始 RGB565，格式与面板线上顺序一致，
 * 所以可以从 Flash 直接送给 SPI，不占 RAM、也不需要色彩转换。
 *
 * (x, y) 以面板可见区左上角为原点，是**物理坐标，不受画布旋转影响**；
 * 矩形必须完整落在屏内，越界返回 TFT_UNSUPPORTED。
 *
 * ⚠️ 调用时不能有正在进行的刷新（否则返回 TFT_BUSY），而且本函数会作废
 * 内部那份「最近稳定帧」：因为这块图像不在帧缓冲里，逐字节比较再也算不出
 * 正确差异，所以下一次正常刷新会强制整屏重发。用它画完再回到 UI 是安全的，
 * 但不要拿它当「局部涂改」的手段。
 */
TFT_Status TFT_DirectBlit(int16_t x, int16_t y, uint16_t width, uint16_t height,
                          const uint8_t *rgb565);

/** 使用 SPI 中断发送寻址命令和页数据，启动异步刷新。 */
TFT_Status TFT_UpdateIT(void);

/** 使用 SPI 中断发送寻址命令、DMA 发送页数据，启动异步刷新。 */
TFT_Status TFT_UpdateDMA(void);

/*
 * 异步传输一旦启动，其冻结缓冲区在完成或失败后都会成为最近稳定可重发帧。
 * “稳定”只表示内存图像完整且不再被绘图修改，不保证失败帧已完整显示在屏幕上。
 */

/** 查询核心或驱动是否仍在执行异步刷新。 */
bool TFT_IsBusy(void);

/** 获取最近一次刷新或控制操作的状态。 */
TFT_Status TFT_GetLastStatus(void);

/** 设置对比度，value 的有效范围为 0～255。 */
TFT_Status TFT_SetContrast(uint8_t value);

/** 进入或退出省电模式；退出时会先完整恢复最近稳定可重发帧。 */
TFT_Status TFT_SetPowerSave(bool enable);

/** 直接开关背光；背光不受总线状态或省电模式之外的因素影响。 */
TFT_Status TFT_SetBacklight(bool enable);

/**
 * 背光亮度量程上限，也就是「最亮」对应的等级。
 *
 * 它和 TFT_SetBacklightLevel() 的入参范围一起构成对外 API 约定，所以定义在
 * 公共头里；硬件侧（TIM8 的 Period）必须跟它对齐，见 TFT_Config.h 的说明。
 */
#define TFT_BACKLIGHT_LEVEL_MAX 100U

/**
 * 设置背光亮度，level 有效范围 0～TFT_BACKLIGHT_LEVEL_MAX（默认 100）。
 * 0 等于熄灭，100 等于最亮，与 TFT_SetBacklight(true) 等效。
 *
 * 背光走的是 TIM8_CH3N 的 PWM，不经过 SPI 总线，所以不需要等异步刷新结束。
 */
TFT_Status TFT_SetBacklightLevel(uint8_t level);

/** 获取当前旋转方向下的逻辑画布宽度。 */
uint16_t TFT_GetWidth(void);

/** 获取当前旋转方向下的逻辑画布高度。 */
uint16_t TFT_GetHeight(void);

/** 将当前绘制缓冲区全部清零。 */
void TFT_Clear(void);

/** 将当前绘制缓冲区全部置一。 */
void TFT_Fill(void);

/*
 * 局部重绘的工作方式（可选，不用则行为与只有 TFT_Clear() 时完全一致）：
 *
 *   1. TFT_BeginFrame(false)             开始一帧，保留上一帧内容；
 *   2. 只修改画面中真正变化的那一小块；
 *   3. TFT_InvalidateRegion(...)         声明改动区（可多次调用取并集）；
 *   4. 交给上层提交（KK_UI 会自动完成）。
 *
 * 提交时核心只发送覆盖声明区的页与列，其余像素沿用上一帧在屏幕上的内容，
 * 因此省掉的是「整屏重画 + 整屏传输」两笔开销。
 */

/**
 * 开始绘制一帧。
 *
 * clear_all 为 true 时清空整个绘制缓冲区，等价于 TFT_Clear()，返回 TFT_OK。
 * 为 false 时保留最近一帧的内容，调用方只需修改画面的一小块，随后用
 * TFT_InvalidateRegion() 声明改动区：
 *   TFT_OK    缓冲区已镜像最新稳定帧，可以做局部重绘；
 *   TFT_BUSY  上一帧仍在异步传输中，稳定帧还没产生，缓冲区内容不可用——
 *             请改用 TFT_BeginFrame(true) 整屏重绘（此时自行清屏会让屏幕残缺）；
 *   TFT_ERROR 屏幕尚未初始化成功，应放弃本帧绘制。
 *
 * 无论返回什么，本调用都会作废上一次的改动区声明。
 */
TFT_Status TFT_BeginFrame(bool clear_all);

/** 只清除 [x,x+width) × [y,y+height) 里的像素；坐标与尺寸安全范围同图元约定。 */
void TFT_ClearRegion(int16_t x, int16_t y, uint16_t width, uint16_t height);

/** 设置画布旋转；同时恢复为全屏裁剪窗口。 */
void TFT_SetRotation(TFT_Rotation rotation);

/** 设置后续图元的全局绘图模式。 */
void TFT_SetDrawMode(TFT_DrawMode mode);

/** 设置文字和位图的背景模式。 */
void TFT_SetBackgroundMode(TFT_BackgroundMode mode);

/** 设置半开裁剪窗口 [x,x+width) × [y,y+height)；宽高超限时保持原窗口不变。 */
void TFT_SetClipWindow(int16_t x, int16_t y, uint16_t width, uint16_t height);

/** 将裁剪窗口恢复为当前逻辑画布的全部区域。 */
void TFT_ResetClipWindow(void);

/**
 * 声明本帧发生变化的一个矩形区域。
 *
 * 它不改变任何像素，只是告诉核心「这一块变了」，让下一次提交只发送覆盖该
 * 矩形的页与列。多次调用会累加成一个并集包围盒；声明只对紧接着提交的那一帧
 * 有效，提交后自动失效。
 *
 * 调用方必须保证声明之外没有像素被改动，否则那些改动不会被发送到屏幕。
 * 拿不准时改用 TFT_InvalidateAll()，或干脆什么都不声明——核心会退化为逐字节
 * 比较新旧两帧，结果依旧正确，只是慢一些。
 */
void TFT_InvalidateRegion(int16_t x, int16_t y, uint16_t width, uint16_t height);

/** 声明整屏都可能变化，等价于不做声明时的自动差量行为。 */
void TFT_InvalidateAll(void);

/*
 * 所有宽、高为 0 的图元均无操作；超过各接口注明上限的尺寸也无操作。
 * 只有处于这些安全范围内的坐标和尺寸才承诺按逻辑画布及裁剪窗口截断。
 */

/** 绘制一个像素。 */
void TFT_DrawPixel(int16_t x, int16_t y);

/** 从 (x,y) 向右绘制 width 个像素；width 不得超过 INT16_MAX。 */
void TFT_DrawHLine(int16_t x, int16_t y, uint16_t width);

/** 从 (x,y) 向下绘制 height 个像素；height 不得超过 INT16_MAX。 */
void TFT_DrawVLine(int16_t x, int16_t y, uint16_t height);

/** 绘制包含两个端点的直线。 */
void TFT_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/** 绘制精确宽高的矩形边框；宽高不得超过 INT16_MAX。 */
void TFT_DrawFrame(int16_t x, int16_t y, uint16_t width, uint16_t height);

/** 绘制精确宽高的实心矩形；宽高不得超过 INT16_MAX。 */
void TFT_DrawBox(int16_t x, int16_t y, uint16_t width, uint16_t height);

/** 绘制圆角矩形边框；宽高服从 INT16_MAX，圆角会先按矩形收缩。 */
void TFT_DrawRFrame(int16_t x, int16_t y, uint16_t width, uint16_t height,
                     uint16_t radius);

/** 绘制实心圆角矩形；宽高服从 INT16_MAX，圆角会先按矩形收缩。 */
void TFT_DrawRBox(int16_t x, int16_t y, uint16_t width, uint16_t height,
                   uint16_t radius);

/** 绘制圆周；半径超过当前逻辑画布较长边时无操作。 */
void TFT_DrawCircle(int16_t x, int16_t y, uint16_t radius);

/** 绘制实心圆；半径超过当前逻辑画布较长边时无操作。 */
void TFT_DrawDisc(int16_t x, int16_t y, uint16_t radius);

/** 绘制椭圆轮廓；任一半径超过逻辑画布较长边时无操作。 */
void TFT_DrawEllipse(int16_t x, int16_t y, uint16_t radius_x, uint16_t radius_y);

/** 绘制实心椭圆；任一半径超过逻辑画布较长边时无操作。 */
void TFT_DrawFilledEllipse(int16_t x, int16_t y, uint16_t radius_x,
                            uint16_t radius_y);

/** 绘制顺时针圆弧；起止角相等表示完整圆，半径服从画布上限。 */
void TFT_DrawArc(int16_t x, int16_t y, uint16_t radius,
                  int16_t start_angle, int16_t end_angle);

/** 绘制三角形边框。 */
void TFT_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2);

/** 使用水平扫描线填充三角形。 */
void TFT_DrawFilledTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2);

/** 绘制 XBM；宽高单边不得超过当前逻辑画布较长边。 */
void TFT_DrawXBM(int16_t x, int16_t y, uint16_t width, uint16_t height,
                  const uint8_t *bitmap);

/**
 * 把 1bpp 行式位图整块拷到画布。
 *
 * 源数据格式与 TFT_DrawXBM 相同：行优先、字节内 LSB-first，每行
 * (width + 7) / 8 字节。stride 传 0 表示按该值自动推算，也可以指定更大的
 * 值以支持带填充的行。
 *
 * 与 TFT_DrawXBM 的差别是把行地址与取样提到了循环外，不再逐像素重算步长，
 * 整帧生产者（相机等）可以明显受益。宽高上限与 TFT_DrawXBM 相同，并且同样
 * 只写入当前裁剪窗口之内的像素。
 */
void TFT_Blit1bpp(int16_t x, int16_t y, uint16_t width, uint16_t height,
                   const uint8_t *bitmap, uint16_t stride);

/**
 * 把 1bpp 位图缩放到指定尺寸后绘制。
 *
 * 源格式与 TFT_DrawXBM 相同（行优先、字节内 LSB-first）；scaled_width /
 * scaled_height 是目标尺寸，(x, y) 是目标矩形的左上角。
 *
 * 采用最近邻反向映射：目标像素直接取对应源像素，不做加权。1bpp 只有亮暗
 * 两种取值，放大时没有灰度可插值，最近邻正是这里想要的效果（几何图标放大
 * 后边缘依旧锐利）。缩小会丢细节，不适合用作缩略图。
 *
 * 缩放的尺寸同样受画布单边上限约束，并只写入当前裁剪窗口之内。
 */
void TFT_DrawXBMScale(int16_t x, int16_t y, uint16_t width, uint16_t height,
                       const uint8_t *bitmap,
                       uint16_t scaled_width, uint16_t scaled_height);

/**
 * 围绕位图内部锚点旋转后，将该锚点放到画布目标坐标。
 * 角度为顺时针整数角度，采用反向映射和最近邻采样。
 * 宽高服从画布单边上限，锚点必须位于源位图内部，否则无操作。
 */
void TFT_DrawRotatedXBM(int16_t target_x, int16_t target_y,
                         uint16_t width, uint16_t height,
                         const uint8_t *bitmap,
                         int16_t anchor_x, int16_t anchor_y,
                         int16_t angle);

/** 设置当前 u8g2 格式字体；传入 NULL 可关闭文字绘制。 */
void TFT_SetFont(const uint8_t *font);

/** 设置字形和字符串的前进方向。 */
void TFT_SetFontDirection(TFT_Rotation direction);

/** 设置文字坐标的垂直参考语义。 */
void TFT_SetFontPosition(TFT_FontPosition position);

/** 设置计算 Top、Bottom、Center 时采用的字体参考高度。 */
void TFT_SetFontRefHeight(TFT_FontRefHeight ref_height);

/** 绘制一个 BMP 字符，返回该字符的 advance。 */
int16_t TFT_DrawGlyph(int16_t x, int16_t y, uint16_t codepoint);

/** 绘制单行 UTF-8 字符串，返回全部字符的 advance 总和。 */
int16_t TFT_DrawUTF8(int16_t x, int16_t y, const char *utf8);

/** 计算 UTF-8 字符串的可见像素宽度。 */
uint16_t TFT_GetUTF8Width(const char *utf8);

/** 获取字符的 advance；缺字时返回空格的 advance。 */
int16_t TFT_GetGlyphAdvance(uint16_t codepoint);

/** 获取当前参考高度模式下的字体上升量。 */
int16_t TFT_GetFontAscent(void);

/** 获取当前参考高度模式下的字体下降量，通常为负数。 */
int16_t TFT_GetFontDescent(void);

#ifdef __cplusplus
}
#endif

#endif

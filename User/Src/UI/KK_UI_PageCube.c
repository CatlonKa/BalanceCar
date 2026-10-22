#include "KK_UI_PageCube.h"

#include "KK_UI_FontZh16.h"
#include "TFT.h"
#include "UIAttitude.h"

#include <stddef.h>

/* 重绘周期：30 fps。脏区按实际包围盒声明，通常比整屏小得多。 */
#define CUBE_PERIOD_MS        33U

/* 画布中心与半边长（像素）。立方体顶点 ±1，乘 HALF 得到像素尺寸。 */
#define CUBE_CX               64
#define CUBE_CY               80
#define CUBE_HALF             24

/* 透视距离（以立方体半边长为单位）：越小透视越夸张 */
#define CUBE_FOCAL            3.5f

/* 车头指示线长度（相对 CUBE_HALF）。1.2 时最远触到 x=7/121、y=23/137，
   不会碰到标题下的分隔线（y=18）与底部提示（y=138）。 */
#define CUBE_HEAD_SCALE       1.2f

/* 包围盒外扩：线宽 1 px + 取整误差 */
#define CUBE_BOUND_MARGIN     2

/* 12 条边的端点索引，顶点按 (x,y,z) 的 ±1 二进制编号 */
static const uint8_t s_edges[12][2] = {
    {0U, 1U}, {2U, 3U}, {4U, 5U}, {6U, 7U},   /* 沿 X */
    {0U, 2U}, {1U, 3U}, {4U, 6U}, {5U, 7U},   /* 沿 Y */
    {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}    /* 沿 Z */
};

static uint32_t s_next_ms;
static bool s_active;

/* 上一帧真正画到的范围。局部重绘保留矩形之外的像素，
   所以必须把它并进本帧的脏区，否则旋转时留下残影。 */
static bool s_has_prev;
static int16_t s_prev_x0;
static int16_t s_prev_y0;
static int16_t s_prev_x1;
static int16_t s_prev_y1;

/* 体坐标 (bx,by,bz) 投影到屏幕（含透视）。length 为像素长度。 */
static void cube_project_point(const float *R, float bx, float by, float bz,
                               float length, int16_t x_offset,
                               int16_t *out_x, int16_t *out_y)
{
    /* 体 → 世界 */
    const float wx = R[0] * bx + R[1] * by + R[2] * bz;
    const float wy = R[3] * bx + R[4] * by + R[5] * bz;
    const float wz = R[6] * bx + R[7] * by + R[8] * bz;

    float depth = CUBE_FOCAL + wy;
    float scale;

    if (depth < 0.6f) {
        depth = 0.6f;
    }
    scale = CUBE_FOCAL / depth;

    /* 屏幕右 = -X（X 是车左侧）；屏幕上 = +Z（屏幕 y 向下，故取负） */
    *out_x = (int16_t)((float)x_offset + (float)CUBE_CX -
                       wx * scale * length);
    *out_y = (int16_t)((float)CUBE_CY - wz * scale * length);
}

/* 第 index 个立方体顶点：bit0=x、bit1=y、bit2=z，0 表示 -1。 */
static void cube_project_vertex(const float *R, uint8_t index, int16_t x_offset,
                                int16_t *out_x, int16_t *out_y)
{
    const float bx = (index & 1U) != 0U ? 1.0f : -1.0f;
    const float by = (index & 2U) != 0U ? 1.0f : -1.0f;
    const float bz = (index & 4U) != 0U ? 1.0f : -1.0f;

    cube_project_point(R, bx, by, bz, (float)CUBE_HALF, x_offset, out_x, out_y);
}

/* 车头方向（机体 -Y）的指示线端点。 */
static void cube_head_point(const float *R, int16_t x_offset,
                            int16_t *out_x, int16_t *out_y)
{
    cube_project_point(R, 0.0f, -1.0f, 0.0f,
                       (float)CUBE_HALF * CUBE_HEAD_SCALE, x_offset,
                       out_x, out_y);
}

/* 本帧需要重画的矩形（屏幕坐标）：8 顶点 + 指示线端点，并入上一帧范围。 */
static bool cube_bounds(const float *R, int16_t *x0, int16_t *y0,
                        int16_t *x1, int16_t *y1)
{
    int16_t left = 32767;
    int16_t top = 32767;
    int16_t right = -32768;
    int16_t bottom = -32768;
    const int16_t screen_w = (int16_t)TFT_GetWidth();
    const int16_t screen_h = (int16_t)TFT_GetHeight();
    int16_t px;
    int16_t py;
    uint8_t i;

    for (i = 0U; i < 8U; ++i) {
        cube_project_vertex(R, i, 0, &px, &py);
        if (px < left) { left = px; }
        if (px > right) { right = px; }
        if (py < top) { top = py; }
        if (py > bottom) { bottom = py; }
    }

    cube_head_point(R, 0, &px, &py);
    if (px < left) { left = px; }
    if (px > right) { right = px; }
    if (py < top) { top = py; }
    if (py > bottom) { bottom = py; }

    if (s_has_prev) {
        if (s_prev_x0 < left) { left = s_prev_x0; }
        if (s_prev_y0 < top) { top = s_prev_y0; }
        if (s_prev_x1 > right) { right = s_prev_x1; }
        if (s_prev_y1 > bottom) { bottom = s_prev_y1; }
    }

    /* 记下本帧内容范围（不含外扩），下一帧靠它清掉旧线 */
    s_has_prev = true;
    s_prev_x0 = left;
    s_prev_y0 = top;
    s_prev_x1 = right;
    s_prev_y1 = bottom;

    left = (int16_t)(left - CUBE_BOUND_MARGIN);
    top = (int16_t)(top - CUBE_BOUND_MARGIN);
    right = (int16_t)(right + CUBE_BOUND_MARGIN);
    bottom = (int16_t)(bottom + CUBE_BOUND_MARGIN);

    if (left < 0) { left = 0; }
    if (top < 0) { top = 0; }
    if (right >= screen_w) { right = (int16_t)(screen_w - 1); }
    if (bottom >= screen_h) { bottom = (int16_t)(screen_h - 1); }

    if (right <= left || bottom <= top) {
        return false;
    }

    *x0 = left;
    *y0 = top;
    *x1 = right;
    *y1 = bottom;
    return true;
}

void KK_UI_PageCube_OnEnter(void)
{
    UIAttitude_Reset();
    s_next_ms = 0U;
    s_active = true;
    s_has_prev = false;
}

void KK_UI_PageCube_OnLeave(void)
{
    s_active = false;
    s_has_prev = false;
}

void KK_UI_PageCube_OnInput(KK_UI_InputEvent event)
{
    /* 归零只绑 KEY1，在 KK_UI_App.c 的 Tick 里消费；界面键在这里不做事。 */
    (void)event;
}

bool KK_UI_PageCube_OnTick(uint32_t now_ms)
{
    float R[9];
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;

    if (!s_active) {
        return false;
    }

    /* 每个主循环都尝试积分：内部比对 imu_seq，没有新样本就立即返回 */
    UIAttitude_Update();

    if (s_next_ms == 0U) {
        s_next_ms = now_ms;
    }
    if ((int32_t)(now_ms - s_next_ms) < 0) {
        return false;
    }
    s_next_ms = now_ms + CUBE_PERIOD_MS;

    UIAttitude_GetMatrix(R);
    if (!cube_bounds(R, &x0, &y0, &x1, &y1)) {
        return false;
    }

    KK_UI_InvalidateRegion(x0, y0, (uint16_t)(x1 - x0), (uint16_t)(y1 - y0));
    return true;
}

void KK_UI_PageCube_OnDraw(int16_t x_offset, int16_t clip_x, uint16_t clip_width)
{
    float R[9];
    int16_t xs[8];
    int16_t ys[8];
    bool partial = KK_UI_GetDirtyRect(NULL, NULL, NULL, NULL);
    int16_t hx;
    int16_t hy;
    uint8_t i;

    UIAttitude_GetMatrix(R);

    TFT_SetDrawMode(TFT_DRAW_SET);
    TFT_SetBackgroundMode(TFT_BG_TRANSPARENT);
    TFT_SetFontDirection(TFT_ROTATION_0);
    TFT_SetFont(kk_ui_font_zh16);
    TFT_SetFontPosition(TFT_FONT_POS_TOP);
    TFT_SetFontRefHeight(TFT_FONT_REF_ALL);
    if (!partial) {
        TFT_SetClipWindow(clip_x, 0, clip_width, TFT_GetHeight());
        /* 标题与底部提示只在整屏绘制时画；字体是子集化的，
           只能用 ui_charset.txt 里的字（姿/归/零/键 都不在）。 */
        TFT_DrawUTF8((int16_t)(x_offset + 48), 1, "角度");
        TFT_DrawHLine((int16_t)(x_offset + 5), 18, 118);
        TFT_DrawUTF8((int16_t)(x_offset + 48), 138, "重置");
    }

    for (i = 0U; i < 8U; ++i) {
        cube_project_vertex(R, i, x_offset, &xs[i], &ys[i]);
    }

    /* 12 条边全画：超出裁剪窗口的部分由 TFT_DrawLine 内部裁剪。
       不要在这里按端点剔除，否则端点出框时整条边会消失。 */
    for (i = 0U; i < 12U; ++i) {
        const uint8_t a = s_edges[i][0];
        const uint8_t b = s_edges[i][1];

        TFT_DrawLine(xs[a], ys[a], xs[b], ys[b]);
    }

    /* 车头指示：机体 -Y 方向，任何角度都能分辨朝向 */
    cube_head_point(R, x_offset, &hx, &hy);
    TFT_DrawLine((int16_t)(x_offset + CUBE_CX), CUBE_CY, hx, hy);
    TFT_DrawPixel(hx, hy);
    TFT_DrawPixel((int16_t)(hx - 1), hy);
    TFT_DrawPixel((int16_t)(hx + 1), hy);

    TFT_ResetClipWindow();
}

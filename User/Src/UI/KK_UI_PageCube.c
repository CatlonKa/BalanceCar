#include "KK_UI_PageCube.h"

#include "KK_UI_FontZh16.h"
#include "TFT.h"
#include "UIAttitude.h"

#include <stddef.h>

/*
 * 观察方向：屏幕装在车尾，人朝车的行进方向看，所以镜头在车尾（+Y），
 * 朝车头（-Y）看。于是：
 *   屏幕右 = -X（X 是车左侧，所以屏幕右 = 车右侧）
 *   屏幕上 = +Z（车顶）
 *   越远越小（depth = FOCAL - wy，wy 越大越靠近镜头）
 * 若实测左右仍镜像，把 CUBE_SCREEN_X_SIGN 改成 +1.0f 即可。
 */
#define CUBE_SCREEN_X_SIGN    (-1.0f)

/* 重绘周期：30 fps。脏区按实际包围盒声明，通常比整屏小得多。 */
#define CUBE_PERIOD_MS        33U

/* 画布中心与半边长（像素）。立方体顶点 ±1，乘 HALF 得到像素尺寸。 */
#define CUBE_CX               64
#define CUBE_CY               78
#define CUBE_HALF             24

/* 透视距离（以立方体半边长为单位）：越小透视越夸张。 */
#define CUBE_FOCAL            3.5f

/*
 * 车头指示：一根方形长方体（炮管），沿机体 -Y 伸出。
 * 尺寸以 CUBE_HALF 为单位（立方体棱长 = 2 个单位）：
 *   内部 0.5 → 棱长的 1/4；总长 1.5 → 棱长的 3/4；伸出 1.0 → 棱长的 1/2
 * 截面是正方形（两个小面），另外四个面是长方形。
 */
#define CUBE_BARREL_HALF      0.25f    /* 截面半宽 */
#define CUBE_BARREL_BASE     (-0.5f)   /* 起点：立方体内部 */
#define CUBE_BARREL_TIP      (-2.0f)   /* 末端：立方体外部 */

/* 包围盒外扩：线宽 1 px + 取整误差 */
#define CUBE_BOUND_MARGIN     2

/*
 * 标定提示条：屏幕中央一整条全宽白底（x=0..127），黑字。
 * 因为它要盖住立方体，所以用独立脏矩形；高度盖得住文字行高（16px）。
 */
#define CUBE_HINT_Y           72      /* 中心 y=78，条高 20 → 72..91 */
#define CUBE_HINT_H           20
#define CUBE_HINT_TEXT_Y      74
#define CUBE_HINT_W           128

/* 「标定中」3 个汉字 = 48 px，后面跟动画字符（16 px），中间留 10 px。
   总宽约 74，居中起点 = (128-74)/2 = 27。 */
#define CUBE_HINT_TEXT_X      27
#define CUBE_HINT_SPIN_X      85

/* 标定每次推进采多少个样本（每个 1 ms）。400/20 = 20 次调用 ≈ 400 ms。 */
#define CUBE_CALIB_BATCH      20U

/* 动画换帧周期：120 ms 一帧，一圈 480 ms，不刺眼 */
#define CUBE_SPIN_PERIOD_MS   120U

/* 旋转动画：竖杠、斜杠、两根连字符、反斜杠。用两个 "-" 而不是破折号 ——
   破折号字形在字格里竖直位置不同，换到它时会上下跳；两根连字符与斜杠同基线，
   而且长度是原来的两倍。 */
static const char *const s_spin[4] = { "|", "/", "--", "\\" };

enum {
    CUBE_MODE_CALIB = 0,    /* 正在标定，提示行动画，立方体不动 */
    CUBE_MODE_RUN           /* 标定完成，立方体跟随姿态 */
};

/* 炮管 8 条可见棱：4 条长棱（底面 → 末端）+ 末端小方框 4 条。
   底面小方框在立方体内部，不画。顶点按 bit0=x、bit1=z、bit2=末端 编号。 */
static const uint8_t s_barrel_edges[8][2] = {
    {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U},   /* 长棱 */
    {4U, 5U}, {5U, 7U}, {7U, 6U}, {6U, 4U}    /* 末端方框 */
};

/* 立方体 12 条边的端点索引，顶点按 (x,y,z) 的 ±1 二进制编号 */
static const uint8_t s_edges[12][2] = {
    {0U, 1U}, {2U, 3U}, {4U, 5U}, {6U, 7U},   /* 沿 X */
    {0U, 2U}, {1U, 3U}, {4U, 6U}, {5U, 7U},   /* 沿 Y */
    {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}    /* 沿 Z */
};

static uint32_t s_next_ms;
static bool s_active;
static uint8_t s_mode;
static uint8_t s_spin_index;
static uint32_t s_spin_next_ms;   /* 下一次换动画字符的时刻 */
static bool s_need_full;    /* 本帧要整屏重画（进页首帧、或刚从标定切回来） */

/* 上一帧真正画到的范围。局部重绘保留矩形之外的像素，
   所以必须把它并进本帧的脏区，否则旋转时留下残影。 */
static bool s_has_prev;
static int16_t s_prev_x0;
static int16_t s_prev_y0;
static int16_t s_prev_x1;
static int16_t s_prev_y1;

/* 体坐标 (bx,by,bz) 投影到屏幕（含透视）。坐标以 CUBE_HALF 为单位。 */
static void cube_project_point(const float *R, float bx, float by, float bz,
                               int16_t x_offset, int16_t *out_x, int16_t *out_y)
{
    /* 体 → 世界 */
    const float wx = R[0] * bx + R[1] * by + R[2] * bz;
    const float wy = R[3] * bx + R[4] * by + R[5] * bz;
    const float wz = R[6] * bx + R[7] * by + R[8] * bz;

    /* 镜头在车尾（+Y）：wy 越大越近，所以用减 */
    float depth = CUBE_FOCAL - wy;
    float scale;

    if (depth < 0.6f) {
        depth = 0.6f;
    }
    scale = CUBE_FOCAL / depth;

    /* 屏幕右 = -X（X 是车左侧）；屏幕上 = +Z（屏幕 y 向下，故取负） */
    *out_x = (int16_t)((float)x_offset + (float)CUBE_CX +
                       CUBE_SCREEN_X_SIGN * wx * scale * (float)CUBE_HALF);
    *out_y = (int16_t)((float)CUBE_CY - wz * scale * (float)CUBE_HALF);
}

/* 第 index 个立方体顶点：bit0=x、bit1=y、bit2=z，0 表示 -1。 */
static void cube_project_vertex(const float *R, uint8_t index, int16_t x_offset,
                                int16_t *out_x, int16_t *out_y)
{
    const float bx = (index & 1U) != 0U ? 1.0f : -1.0f;
    const float by = (index & 2U) != 0U ? 1.0f : -1.0f;
    const float bz = (index & 4U) != 0U ? 1.0f : -1.0f;

    cube_project_point(R, bx, by, bz, x_offset, out_x, out_y);
}

/*
 * 炮管 8 个顶点：bit0 = 截面 x 方向、bit1 = 截面 z 方向、bit2 = 0 底面 / 1 末端。
 * 沿 -Y 伸出，所以截面在 X-Z 平面内。
 */
static void cube_project_barrel(const float *R, uint8_t index, int16_t x_offset,
                                int16_t *out_x, int16_t *out_y)
{
    const float bx = ((index & 1U) != 0U ? 1.0f : -1.0f) * CUBE_BARREL_HALF;
    const float bz = ((index & 2U) != 0U ? 1.0f : -1.0f) * CUBE_BARREL_HALF;
    const float by = ((index & 4U) != 0U) ? CUBE_BARREL_TIP : CUBE_BARREL_BASE;

    cube_project_point(R, bx, by, bz, x_offset, out_x, out_y);
}

/* 把一个点并入包围盒。 */
static void cube_bounds_add(int16_t px, int16_t py,
                            int16_t *left, int16_t *top,
                            int16_t *right, int16_t *bottom)
{
    if (px < *left) { *left = px; }
    if (px > *right) { *right = px; }
    if (py < *top) { *top = py; }
    if (py > *bottom) { *bottom = py; }
}

/* 本帧需要重画的矩形：立方体 8 顶点 + 炮管 8 顶点，并入上一帧范围。 */
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
        cube_bounds_add(px, py, &left, &top, &right, &bottom);

        cube_project_barrel(R, i, 0, &px, &py);
        cube_bounds_add(px, py, &left, &top, &right, &bottom);
    }

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

/* 只声明标定提示条：标定期间立方体不动，不用重画 */
static void cube_invalidate_hint(void)
{
    KK_UI_InvalidateRegion(0, CUBE_HINT_Y, CUBE_HINT_W, CUBE_HINT_H);
}

void KK_UI_PageCube_RestartCalib(void)
{
    /* 按 KEY1：重新标定零偏，并把当前水平朝向设为基准 */
    UIAttitude_CalibStart();
    s_mode = CUBE_MODE_CALIB;
    s_spin_index = 0U;
    s_spin_next_ms = 0U;
    s_need_full = true;
    s_next_ms = 0U;

    if (s_active) {
        KK_UI_InvalidateRegion(0, 0, TFT_GetWidth(), TFT_GetHeight());
    }
}

void KK_UI_PageCube_OnEnter(void)
{
    s_active = true;
    s_has_prev = false;
    KK_UI_PageCube_RestartCalib();
}

void KK_UI_PageCube_OnLeave(void)
{
    s_active = false;
    s_has_prev = false;
}

void KK_UI_PageCube_OnInput(KK_UI_InputEvent event)
{
    /* 归零/重标只绑 KEY1，在 KK_UI_App.c 的 Tick 里消费；界面键在这里不做事。 */
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

    /* ---------------- 标定中：每帧推进一步，按 120 ms 换动画字符 ---------------- */
    if (s_mode == CUBE_MODE_CALIB) {
        UI_ATTITUDE_CalibStatus status = UIAttitude_CalibStep(CUBE_CALIB_BATCH);

        if (status == UI_ATTITUDE_CALIB_BUSY) {
            bool spin_changed = false;

            if (s_spin_next_ms == 0U ||
                (int32_t)(now_ms - s_spin_next_ms) >= 0) {
                s_spin_next_ms = now_ms + CUBE_SPIN_PERIOD_MS;
                s_spin_index = (uint8_t)((s_spin_index + 1U) & 3U);
                spin_changed = true;
            }

            if (s_need_full) {
                /* 进页首帧：整屏画标题 + 初始姿态立方体 + 提示条 */
                s_need_full = false;
                KK_UI_InvalidateRegion(0, 0, TFT_GetWidth(), TFT_GetHeight());
                return true;
            }
            if (spin_changed) {
                cube_invalidate_hint();
                return true;
            }
            return false;
        }

        /* 标定结束（DONE 或 FAILED）：清掉提示条，下一帧开始跟随姿态 */
        s_mode = CUBE_MODE_RUN;
        s_next_ms = 0U;
        s_need_full = true;
        cube_invalidate_hint();
        return true;
    }

    /* ---------------- 标定完成：跟随姿态 ---------------- */
    UIAttitude_Update();

    if (s_need_full) {
        s_need_full = false;
        s_next_ms = now_ms + CUBE_PERIOD_MS;
        KK_UI_InvalidateRegion(0, 0, TFT_GetWidth(), TFT_GetHeight());
        return true;
    }

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
    int16_t bxs[8];
    int16_t bys[8];
    bool partial = KK_UI_GetDirtyRect(NULL, NULL, NULL, NULL);
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
    }

    /* 标题。局部重绘时会被裁剪窗口裁掉，自动保留上一帧。 */
    TFT_DrawUTF8((int16_t)(x_offset + 48), 1, "姿态");
    TFT_DrawHLine((int16_t)(x_offset + 5), 18, 118);

    for (i = 0U; i < 8U; ++i) {
        cube_project_vertex(R, i, x_offset, &xs[i], &ys[i]);
        cube_project_barrel(R, i, x_offset, &bxs[i], &bys[i]);
    }

    /*
     * 先画炮管、再画立方体：立方体的棱盖在炮管上，符合「炮管从立方体内部
     * 长出来」的遮挡关系。超出裁剪窗口的部分由 TFT_DrawLine 内部裁剪。
     */
    for (i = 0U; i < 8U; ++i) {
        const uint8_t a = s_barrel_edges[i][0];
        const uint8_t b = s_barrel_edges[i][1];

        TFT_DrawLine(bxs[a], bys[a], bxs[b], bys[b]);
    }

    /* 立方体 12 条棱 */
    for (i = 0U; i < 12U; ++i) {
        const uint8_t a = s_edges[i][0];
        const uint8_t b = s_edges[i][1];

        TFT_DrawLine(xs[a], ys[a], xs[b], ys[b]);
    }

    /*
     * 标定提示条：白底黑字，从最左到最右，盖在立方体上。
     *
     * 极性：帧缓冲里 0 = 黑、1 = 白（TFT_COLOR_BACKGROUND / FOREGROUND）。
     * 所以「白底」要用 SET 填 1，「黑字」要用 CLEAR 写 0。
     * 顺序不能反：先填白再写字，并且填白会把上面画过的立方体线盖掉。
     */
    if (s_mode == CUBE_MODE_CALIB) {
        /* 1) 整条填白 */
        TFT_SetDrawMode(TFT_DRAW_SET);
        TFT_DrawBox(x_offset, CUBE_HINT_Y, CUBE_HINT_W, CUBE_HINT_H);

        /* 2) 黑字：CLEAR 模式 + 透明背景，只把字形像素写 0 */
        TFT_SetDrawMode(TFT_DRAW_CLEAR);
        TFT_SetBackgroundMode(TFT_BG_TRANSPARENT);
        TFT_SetFontDirection(TFT_ROTATION_0);
        TFT_SetFont(kk_ui_font_zh16);
        TFT_SetFontPosition(TFT_FONT_POS_TOP);
        TFT_SetFontRefHeight(TFT_FONT_REF_ALL);

        TFT_DrawUTF8((int16_t)(x_offset + CUBE_HINT_TEXT_X), CUBE_HINT_TEXT_Y,
                     "标定中");
        TFT_DrawUTF8((int16_t)(x_offset + CUBE_HINT_SPIN_X), CUBE_HINT_TEXT_Y,
                     s_spin[s_spin_index]);

        /* 3) 恢复前景模式，别影响后续绘制 */
        TFT_SetDrawMode(TFT_DRAW_SET);
    } else {
        /* 底部提示：KEY1 重新标定（= 重新归零水平朝向） */
        TFT_SetDrawMode(TFT_DRAW_SET);
        TFT_SetBackgroundMode(TFT_BG_TRANSPARENT);
        TFT_DrawUTF8((int16_t)(x_offset + 48), 140, "重置");
    }

    TFT_ResetClipWindow();
}

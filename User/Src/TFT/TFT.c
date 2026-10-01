#include "TFT_Internal.h"

#include <limits.h>
#include <string.h>

/* 核心层负责双缓冲、差分提交、画布状态和逻辑像素到物理显存的映射。 */

/** 刷新请求使用的底层传输方式。 */
typedef enum {
    TFT_UPDATE_BLOCKING = 0, /**< 阻塞发送命令和数据。 */
    TFT_UPDATE_IT,           /**< 命令、数据均使用中断发送。 */
    TFT_UPDATE_DMA           /**< 命令用中断，数据用 DMA。 */
} TFT_UpdateMode;

static uint8_t tft_buffers[2][TFT_BUFFER_SIZE]; /**< 库内静态双缓冲，按页连续存放。 */
static uint8_t tft_stable_index;                /**< 最近稳定可完整重发帧的缓冲区索引。 */
static uint8_t tft_draw_index = 1U;             /**< 用户当前正在绘制的缓冲区索引。 */
static uint8_t tft_transfer_index = 1U;         /**< 本次传输期间被冻结的缓冲区索引。 */
static uint8_t tft_dirty_min[TFT_PHYSICAL_PAGES]; /**< 每页首个差异列。 */
static uint8_t tft_dirty_max[TFT_PHYSICAL_PAGES]; /**< 每页最后一个差异列。 */
static volatile bool tft_async_pending;         /**< 核心是否等待异步传输完成。 */
static bool tft_force_full;                     /**< 下次刷新是否必须发送全屏。 */
static bool tft_initialized;                    /**< 屏幕和核心状态是否初始化成功。 */
static bool tft_power_save;                     /**< 当前是否处于显示关闭状态。 */
static volatile TFT_Status tft_last_status = TFT_OK; /**< 最近操作的可查询状态。 */

#if TFT_RETAIN_FRAME
static bool tft_retain_pending; /**< 异步提交后等待把最新稳定帧镜像回绘制缓冲区。 */
#endif
static bool tft_dirty_declared; /**< 本帧改动区是否由调用方声明。 */
static int16_t tft_decl_x0;     /**< 声明区左边界，包含。 */
static int16_t tft_decl_y0;     /**< 声明区上边界，包含。 */
static int16_t tft_decl_x1;     /**< 声明区右边界，不包含。 */
static int16_t tft_decl_y1;     /**< 声明区下边界，不包含。 */

static TFT_Rotation tft_rotation = TFT_ROTATION_0; /**< 当前画布旋转方向。 */
static TFT_DrawMode tft_draw_mode = TFT_DRAW_SET;  /**< 当前前景像素写入方式。 */
static TFT_BackgroundMode tft_background_mode = TFT_BG_TRANSPARENT; /**< 背景写入方式。 */
static int16_t tft_clip_x0; /**< 裁剪窗口左边界，包含该列。 */
static int16_t tft_clip_y0; /**< 裁剪窗口上边界，包含该行。 */
static int16_t tft_clip_x1 = (int16_t)TFT_PHYSICAL_WIDTH;  /**< 裁剪窗口右边界，不包含。 */
static int16_t tft_clip_y1 = (int16_t)TFT_PHYSICAL_HEIGHT; /**< 裁剪窗口下边界，不包含。 */

/** 将全部页标记为“没有差异”。 */
static void tft_reset_dirty(void)
{
    uint8_t page;

    for (page = 0U; page < TFT_PHYSICAL_PAGES; ++page) {
        tft_dirty_min[page] = TFT_PHYSICAL_WIDTH;
        tft_dirty_max[page] = 0U;
    }
}

/** 把一个逻辑坐标点映射成物理显存坐标，与 TFT_InternalPlotSource 保持一致。 */
static void tft_map_logical_point(int16_t x, int16_t y,
                                  int16_t *physical_x, int16_t *physical_y)
{
    switch (tft_rotation) {
    case TFT_ROTATION_90:
        *physical_x = (int16_t)(TFT_PHYSICAL_WIDTH - 1U) - y;
        *physical_y = x;
        break;
    case TFT_ROTATION_180:
        *physical_x = (int16_t)(TFT_PHYSICAL_WIDTH - 1U) - x;
        *physical_y = (int16_t)(TFT_PHYSICAL_HEIGHT - 1U) - y;
        break;
    case TFT_ROTATION_270:
        *physical_x = y;
        *physical_y = (int16_t)(TFT_PHYSICAL_HEIGHT - 1U) - x;
        break;
    case TFT_ROTATION_0:
    default:
        *physical_x = x;
        *physical_y = y;
        break;
    }
}

/** 把调用方声明的逻辑矩形映射成每页的首尾差异列。 */
static bool tft_apply_declared_dirty(void)
{
    int16_t corner_x[2]; /* 声明矩形两个对角点的物理列。 */
    int16_t corner_y[2]; /* 声明矩形两个对角点的物理行。 */
    int16_t pixel_min_x;
    int16_t pixel_max_x;
    int16_t pixel_min_y;
    int16_t pixel_max_y;
    uint8_t first_page;
    uint8_t last_page;
    uint8_t page;

    tft_map_logical_point(tft_decl_x0, tft_decl_y0, &corner_x[0], &corner_y[0]);
    tft_map_logical_point((int16_t)(tft_decl_x1 - 1), (int16_t)(tft_decl_y1 - 1),
                          &corner_x[1], &corner_y[1]);

    /* 四种旋转都把轴对齐矩形映射成轴对齐矩形，取两个对角点的极值即可。 */
    pixel_min_x = (corner_x[0] < corner_x[1]) ? corner_x[0] : corner_x[1];
    pixel_max_x = (corner_x[0] > corner_x[1]) ? corner_x[0] : corner_x[1];
    pixel_min_y = (corner_y[0] < corner_y[1]) ? corner_y[0] : corner_y[1];
    pixel_max_y = (corner_y[0] > corner_y[1]) ? corner_y[0] : corner_y[1];

    if (pixel_min_x < 0) {
        pixel_min_x = 0;
    }
    if (pixel_min_y < 0) {
        pixel_min_y = 0;
    }
    if (pixel_max_x > (int16_t)(TFT_PHYSICAL_WIDTH - 1U)) {
        pixel_max_x = (int16_t)(TFT_PHYSICAL_WIDTH - 1U);
    }
    if (pixel_max_y > (int16_t)(TFT_PHYSICAL_HEIGHT - 1U)) {
        pixel_max_y = (int16_t)(TFT_PHYSICAL_HEIGHT - 1U);
    }
    if (pixel_min_x > pixel_max_x || pixel_min_y > pixel_max_y) {
        return false;
    }

    /* 声明区可能跨页，每页都发该矩形所覆盖的列区间。 */
    first_page = (uint8_t)((uint16_t)pixel_min_y >> 3U);
    last_page = (uint8_t)((uint16_t)pixel_max_y >> 3U);
    for (page = first_page; page <= last_page; ++page) {
        tft_dirty_min[page] = (uint8_t)pixel_min_x;
        tft_dirty_max[page] = (uint8_t)pixel_max_x;
    }
    return true;
}

/** 比较新旧帧，为每页生成一个首尾差异列区间。 */
static bool tft_prepare_dirty(uint8_t new_index, uint8_t old_index, bool full)
{
    uint8_t page;    /* 当前扫描的物理页。 */
    bool any = false; /* 是否至少存在一个需要发送的页。 */

    tft_reset_dirty();
    /* 调用方声明了改动区时直接按声明生成，不必再逐字节比较两帧。 */
    if (!full && tft_dirty_declared) {
        return tft_apply_declared_dirty();
    }
    for (page = 0U; page < TFT_PHYSICAL_PAGES; ++page) {
        uint16_t base = (uint16_t)page * TFT_PHYSICAL_WIDTH; /* 当前页首字节。 */
        uint16_t x; /* 当前比较列。 */

        /* 传输失败后的恢复刷新直接覆盖整页，不再比较旧帧。 */
        if (full) {
            tft_dirty_min[page] = 0U;
            tft_dirty_max[page] = TFT_PHYSICAL_WIDTH - 1U;
            any = true;
            continue;
        }

        /* 从左、右两端分别寻找差异，得到本页唯一连续发送区间。 */
        for (x = 0U; x < TFT_PHYSICAL_WIDTH; ++x) {
            if (tft_buffers[new_index][base + x] != tft_buffers[old_index][base + x]) {
                tft_dirty_min[page] = (uint8_t)x;
                break;
            }
        }
        if (x == TFT_PHYSICAL_WIDTH) {
            continue;
        }
        for (x = TFT_PHYSICAL_WIDTH; x > 0U; --x) {
            uint16_t column = x - 1U;
            if (tft_buffers[new_index][base + column] != tft_buffers[old_index][base + column]) {
                tft_dirty_max[page] = (uint8_t)column;
                break;
            }
        }
        any = true;
    }
    return any;
}

/** 完成同步或空差异帧提交，并准备下一帧的绘制缓冲区。 */
static void tft_commit_blocking(void)
{
    uint8_t old_stable = tft_stable_index; /* 提交后复用为下一帧绘制缓冲区。 */

    /* 新帧已经完整发出，成为稳定重发基准，可以直接镜像给绘制缓冲区。 */
    tft_stable_index = tft_draw_index;
    tft_draw_index = old_stable;
#if TFT_RETAIN_FRAME
    memcpy(tft_buffers[tft_draw_index], tft_buffers[tft_stable_index],
           TFT_BUFFER_SIZE);
    tft_retain_pending = false;
#else
    memset(tft_buffers[tft_draw_index], 0, TFT_BUFFER_SIZE);
#endif
    tft_force_full = false;
    tft_last_status = TFT_OK;
}

/** 统一处理三种刷新入口的差分准备、驱动启动和缓冲区交换。 */
static TFT_Status tft_begin_update(TFT_UpdateMode mode)
{
    TFT_Status status;      /* 驱动启动或阻塞传输结果。 */
    uint8_t previous_draw;  /* 异步启动失败时用来恢复绘制缓冲区索引。 */

    if (!tft_initialized) {
        tft_last_status = TFT_ERROR;
        return TFT_ERROR;
    }
    if (TFT_IsBusy()) {
        return TFT_BUSY;
    }

    /* 在接触总线前固定传输帧并计算各页差异区间。 */
    tft_transfer_index = tft_draw_index;
    if (!tft_prepare_dirty(tft_transfer_index, tft_stable_index, tft_force_full)) {
        /* 空差异帧不访问总线，但仍按正常帧完成缓冲区交换。 */
        tft_commit_blocking();
        return TFT_OK;
    }

    if (mode == TFT_UPDATE_BLOCKING) {
        status = TFT_DriverWriteBlocking();
        if (status == TFT_OK) {
            tft_commit_blocking();
        } else {
            /* 屏幕可能只收到半帧，下次必须完整覆盖。 */
            tft_force_full = true;
            tft_last_status = status;
        }
        return status;
    }

    /*
     * 缓冲区交换和异步标志必须在启动驱动之前完成。
     *
     * 驱动一启动就可能进入中断，而完成通知会在中断里读 tft_async_pending：
     * 若那时标志还没置位，完成通知会因“没有待完成传输”被丢弃，
     * 随后这里再把它置位，就再没有任何人会清除它，
     * TFT_IsBusy() 会永久返回 true。
     */
    previous_draw = tft_draw_index;
    tft_draw_index = tft_stable_index;
#if !TFT_RETAIN_FRAME
    memset(tft_buffers[tft_draw_index], 0, TFT_BUFFER_SIZE);
#endif
    tft_async_pending = true;
#if TFT_RETAIN_FRAME
    /*
     * 此刻 tft_stable_index 还指向上一帧，要等本次传输完成后才指向刚发出的帧，
     * 所以镜像只能在任务上下文里做。这里只记下待办，由 TFT_BeginFrame() 补齐。
     */
    tft_retain_pending = true;
#endif

    status = (mode == TFT_UPDATE_IT) ? TFT_DriverWriteIT() : TFT_DriverWriteDMA();
    if (status != TFT_OK) {
        /* 驱动没能启动。若完成通知已经处理过这次传输，就不要回退缓冲区索引。 */
        if (tft_async_pending) {
            tft_async_pending = false;
            tft_draw_index = previous_draw;
#if TFT_RETAIN_FRAME
            /* 没发出去的帧还留在绘制缓冲区里，不能用稳定帧覆盖它。 */
            tft_retain_pending = false;
#endif
        }
        tft_force_full = true;
        tft_last_status = status;
        return status;
    }
    if (!tft_async_pending) {
        /* 传输在驱动返回之前就完成了，状态已由完成通知收尾。 */
        return TFT_OK;
    }
    if (!TFT_DriverIsBusy()) {
        /*
         * 驱动接受了请求但没有真正启动传输（没有差异页），不会有完成通知。
         * 这里必须自行收尾，否则待完成标志无人清除。
         * 这一帧的内容没有发出去，所以恢复绘制缓冲区索引保留它，等下次刷新再发。
         */
        tft_async_pending = false;
        tft_draw_index = previous_draw;
#if TFT_RETAIN_FRAME
        tft_retain_pending = false;
#endif
        tft_last_status = TFT_OK;
        return TFT_OK;
    }
    tft_last_status = TFT_BUSY;
    return TFT_OK;
}

TFT_Status TFT_Init(void)
{
    TFT_Status status; /* 屏幕驱动初始化结果。 */

    if (TFT_IsBusy()) {
        return TFT_BUSY;
    }

    /* 恢复所有核心状态，保证重复初始化也从确定的空白帧开始。 */
    memset(tft_buffers, 0, sizeof(tft_buffers));
    tft_stable_index = 0U;
    tft_draw_index = 1U;
    tft_transfer_index = 1U;
    tft_async_pending = false;
    tft_force_full = false;
    tft_power_save = false;
    tft_retain_pending = false;
    tft_dirty_declared = false;
    tft_rotation = TFT_ROTATION_0;
    tft_draw_mode = TFT_DRAW_SET;
    tft_background_mode = TFT_BG_TRANSPARENT;
    TFT_ResetClipWindow();

    status = TFT_DriverInit();
    tft_initialized = (status == TFT_OK);
    tft_last_status = status;
    return status;
}

void TFT_DeInit(void)
{
    /*
     * 屏幕已经不在（拔掉或断电），先把驱动层收干净：它会停背光 PWM，
     * 但不会向 SPI 发任何命令。
     */
    TFT_DriverDeInit();

    /* 状态复位与 TFT_Init() 一致，保证下一次初始化从确定的空白帧开始。 */
    memset(tft_buffers, 0, sizeof(tft_buffers));
    tft_stable_index = 0U;
    tft_draw_index = 1U;
    tft_transfer_index = 1U;
    tft_async_pending = false;
    tft_force_full = false;
    tft_power_save = false;
#if TFT_RETAIN_FRAME
    tft_retain_pending = false;
#endif
    tft_dirty_declared = false;
    tft_rotation = TFT_ROTATION_0;
    tft_draw_mode = TFT_DRAW_SET;
    tft_background_mode = TFT_BG_TRANSPARENT;
    TFT_ResetClipWindow();

    tft_initialized = false;
    tft_last_status = TFT_OK;
}

TFT_Status TFT_Update(void)
{
    return tft_begin_update(TFT_UPDATE_BLOCKING);
}

TFT_Status TFT_UpdateIT(void)
{
    return tft_begin_update(TFT_UPDATE_IT);
}

TFT_Status TFT_UpdateDMA(void)
{
    return tft_begin_update(TFT_UPDATE_DMA);
}

bool TFT_IsBusy(void)
{
    return tft_async_pending || TFT_DriverIsBusy();
}

TFT_Status TFT_GetLastStatus(void)
{
    return tft_last_status;
}

TFT_Status TFT_SetContrast(uint8_t value)
{
    TFT_Status status;

    if (TFT_IsBusy()) {
        return TFT_BUSY;
    }
    status = TFT_DriverSetContrast(value);
    tft_last_status = status;
    return status;
}

TFT_Status TFT_SetPowerSave(bool enable)
{
    TFT_Status status; /* 关屏命令、恢复帧或开屏命令的结果。 */

    if (TFT_IsBusy()) {
        return TFT_BUSY;
    }
    if (!tft_initialized) {
        tft_last_status = TFT_ERROR;
        return TFT_ERROR;
    }
    if (enable == tft_power_save) {
        tft_last_status = TFT_OK;
        return TFT_OK;
    }

    if (enable) {
        status = TFT_DriverSetPowerSave(true);
        if (status == TFT_OK) {
            tft_power_save = true;
        }
        tft_last_status = status;
        return status;
    }

    /* 显示保持关闭，先完整恢复最近稳定帧，再发送 AF 点亮。 */
    tft_transfer_index = tft_stable_index;
    (void)tft_prepare_dirty(tft_transfer_index, tft_transfer_index, true);
    status = TFT_DriverWriteBlocking();
    if (status == TFT_OK) {
        status = TFT_DriverSetPowerSave(false);
    }
    if (status == TFT_OK) {
        tft_power_save = false;
        /* 唤醒重发不消费异步错误约定的“下一次正常刷新全刷”标志。 */
    } else {
        tft_force_full = true;
    }
    tft_last_status = status;
    return status;
}

TFT_Status TFT_SetBacklight(bool enable)
{
    /* 背光不经过总线，因此不需要等待异步刷新结束。 */
    TFT_Status status = TFT_DriverSetBacklight(enable);

    tft_last_status = status;
    return status;
}

TFT_Status TFT_SetBacklightLevel(uint8_t level)
{
    /* 背光不经过总线，因此不需要等待异步刷新结束。 */
    TFT_Status status = TFT_DriverSetBacklightLevel(level);

    tft_last_status = status;
    return status;
}

uint16_t TFT_GetWidth(void)
{
    return TFT_InternalGetLogicalWidth();
}

uint16_t TFT_GetHeight(void)
{
    return TFT_InternalGetLogicalHeight();
}

void TFT_Clear(void)
{
#if TFT_RETAIN_FRAME
    /* 调用方显式决定了缓冲区内容，取消待镜像，避免下一次开始帧时被覆盖。 */
    tft_retain_pending = false;
#endif
    memset(tft_buffers[tft_draw_index], 0, TFT_BUFFER_SIZE);
}

void TFT_Fill(void)
{
#if TFT_RETAIN_FRAME
    tft_retain_pending = false;
#endif
    memset(tft_buffers[tft_draw_index], 0xFF, TFT_BUFFER_SIZE);
}

TFT_Status TFT_BeginFrame(bool clear_all)
{
    /* 改动区的声明只对紧接着提交的这一帧有效。 */
    tft_dirty_declared = false;
    tft_decl_x0 = 0;
    tft_decl_y0 = 0;
    tft_decl_x1 = 0;
    tft_decl_y1 = 0;

    if (!tft_initialized) {
        tft_last_status = TFT_ERROR;
        return TFT_ERROR;
    }

    if (clear_all) {
#if TFT_RETAIN_FRAME
        tft_retain_pending = false;
#endif
        memset(tft_buffers[tft_draw_index], 0, TFT_BUFFER_SIZE);
        tft_last_status = TFT_OK;
        return TFT_OK;
    }

#if TFT_RETAIN_FRAME
    if (tft_retain_pending) {
        if (TFT_IsBusy()) {
            /*
             * 上一帧还在总线上，最新稳定帧尚未产生，无法镜像。
             * 这里不动缓冲区、也不消费待办，直接告诉调用方改用整屏重绘：
             * 自己清屏会让屏幕只剩下一小块内容。
             */
            tft_last_status = TFT_BUSY;
            return TFT_BUSY;
        }
        memcpy(tft_buffers[tft_draw_index], tft_buffers[tft_stable_index],
               TFT_BUFFER_SIZE);
        tft_retain_pending = false;
    }
#endif

    tft_last_status = TFT_OK;
    return TFT_OK;
}

void TFT_ClearRegion(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    TFT_DrawMode saved_mode = tft_draw_mode; /* 清除过程临时改成 CLEAR。 */
    int16_t saved_clip[4];                   /* 进入前的裁剪窗口。 */
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int16_t plot_x; /* 当前清除列。 */
    int16_t plot_y; /* 当前清除行。 */

    if (width == 0U || height == 0U ||
        width > (uint16_t)INT16_MAX || height > (uint16_t)INT16_MAX) {
        return;
    }
    TFT_InternalGetClip(&saved_clip[0], &saved_clip[1],
                        &saved_clip[2], &saved_clip[3]);
    x0 = x;
    y0 = y;
    x1 = (int32_t)x + width;
    y1 = (int32_t)y + height;
    if (!TFT_InternalIntersectClip(&x0, &y0, &x1, &y1)) {
        return;
    }

    /* 先在窗口内逐点清除，再原样恢复调用方的裁剪窗口。 */
    TFT_SetClipWindow((int16_t)x0, (int16_t)y0,
                      (uint16_t)(x1 - x0), (uint16_t)(y1 - y0));
    tft_draw_mode = TFT_DRAW_CLEAR;
    for (plot_y = (int16_t)y0; plot_y < (int16_t)y1; ++plot_y) {
        for (plot_x = (int16_t)x0; plot_x < (int16_t)x1; ++plot_x) {
            TFT_InternalPlot(plot_x, plot_y);
        }
    }
    tft_draw_mode = saved_mode;
    if (saved_clip[2] > saved_clip[0] && saved_clip[3] > saved_clip[1]) {
        TFT_SetClipWindow(saved_clip[0], saved_clip[1],
                          (uint16_t)(saved_clip[2] - saved_clip[0]),
                          (uint16_t)(saved_clip[3] - saved_clip[1]));
    } else {
        TFT_ResetClipWindow();
    }
}

void TFT_InvalidateRegion(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    int32_t logical_width = TFT_InternalGetLogicalWidth();   /* 当前逻辑宽度。 */
    int32_t logical_height = TFT_InternalGetLogicalHeight(); /* 当前逻辑高度。 */
    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = (int32_t)x + width;
    int32_t y1 = (int32_t)y + height;

    if (width == 0U || height == 0U ||
        width > (uint16_t)INT16_MAX || height > (uint16_t)INT16_MAX) {
        return;
    }

    /* 先收敛到逻辑画布内，再与已有声明取并集。 */
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > logical_width) {
        x1 = logical_width;
    }
    if (y1 > logical_height) {
        y1 = logical_height;
    }
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    if (tft_dirty_declared) {
        if (x0 < tft_decl_x0) {
            tft_decl_x0 = (int16_t)x0;
        }
        if (y0 < tft_decl_y0) {
            tft_decl_y0 = (int16_t)y0;
        }
        if (x1 > tft_decl_x1) {
            tft_decl_x1 = (int16_t)x1;
        }
        if (y1 > tft_decl_y1) {
            tft_decl_y1 = (int16_t)y1;
        }
    } else {
        tft_decl_x0 = (int16_t)x0;
        tft_decl_y0 = (int16_t)y0;
        tft_decl_x1 = (int16_t)x1;
        tft_decl_y1 = (int16_t)y1;
        tft_dirty_declared = true;
    }
}

void TFT_InvalidateAll(void)
{
    /* 交给核心逐字节比较新旧两帧：比「整屏全发」更省，结果同样正确。 */
    tft_dirty_declared = false;
    tft_decl_x0 = 0;
    tft_decl_y0 = 0;
    tft_decl_x1 = 0;
    tft_decl_y1 = 0;
}

void TFT_SetRotation(TFT_Rotation rotation)
{
    if (rotation > TFT_ROTATION_270) {
        rotation = TFT_ROTATION_0;
    }
    tft_rotation = rotation;
    TFT_ResetClipWindow();
}

void TFT_SetDrawMode(TFT_DrawMode mode)
{
    if (mode <= TFT_DRAW_XOR) {
        tft_draw_mode = mode;
    }
}

void TFT_SetBackgroundMode(TFT_BackgroundMode mode)
{
    if (mode <= TFT_BG_SOLID) {
        tft_background_mode = mode;
    }
}

void TFT_SetClipWindow(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    int32_t x1 = (int32_t)x + width; /* 请求窗口的半开右边界。 */
    int32_t y1 = (int32_t)y + height; /* 请求窗口的半开下边界。 */
    int32_t logical_width = TFT_InternalGetLogicalWidth();   /* 当前逻辑宽度。 */
    int32_t logical_height = TFT_InternalGetLogicalHeight(); /* 当前逻辑高度。 */

    if (width > (uint16_t)INT16_MAX || height > (uint16_t)INT16_MAX) {
        return;
    }

    /* 先裁剪左上角，再裁剪右下角，并保持窗口不会出现反向区间。 */
    if (x < 0) {
        tft_clip_x0 = 0;
    } else if (x > logical_width) {
        tft_clip_x0 = (int16_t)logical_width;
    } else {
        tft_clip_x0 = x;
    }
    if (y < 0) {
        tft_clip_y0 = 0;
    } else if (y > logical_height) {
        tft_clip_y0 = (int16_t)logical_height;
    } else {
        tft_clip_y0 = y;
    }

    if (x1 < tft_clip_x0) {
        x1 = tft_clip_x0;
    }
    if (y1 < tft_clip_y0) {
        y1 = tft_clip_y0;
    }
    if (x1 > logical_width) {
        x1 = logical_width;
    }
    if (y1 > logical_height) {
        y1 = logical_height;
    }
    tft_clip_x1 = (int16_t)x1;
    tft_clip_y1 = (int16_t)y1;
}

void TFT_ResetClipWindow(void)
{
    tft_clip_x0 = 0;
    tft_clip_y0 = 0;
    tft_clip_x1 = (int16_t)TFT_InternalGetLogicalWidth();
    tft_clip_y1 = (int16_t)TFT_InternalGetLogicalHeight();
}

void TFT_InternalPlotSource(int16_t x, int16_t y, bool source_pixel)
{
    int16_t physical_x; /* 旋转映射后的物理列。 */
    int16_t physical_y; /* 旋转映射后的物理行。 */
    uint16_t index;     /* 页式缓冲区中的字节下标。 */
    uint8_t mask;       /* 该字节内对应物理行的位掩码。 */
    uint8_t *value;     /* 当前绘制缓冲区的目标字节。 */

    /* 所有上层图元统一在这里完成半开区间裁剪。 */
    if (x < tft_clip_x0 || x >= tft_clip_x1 || y < tft_clip_y0 || y >= tft_clip_y1) {
        return;
    }
    if (!source_pixel) {
        if (tft_background_mode == TFT_BG_TRANSPARENT || tft_draw_mode == TFT_DRAW_XOR) {
            return;
        }
    }

    /* 将旋转后的逻辑画布坐标还原到固定的 128×64 物理坐标。 */
    switch (tft_rotation) {
    case TFT_ROTATION_90:
        physical_x = (int16_t)(TFT_PHYSICAL_WIDTH - 1U) - y;
        physical_y = x;
        break;
    case TFT_ROTATION_180:
        physical_x = (int16_t)(TFT_PHYSICAL_WIDTH - 1U) - x;
        physical_y = (int16_t)(TFT_PHYSICAL_HEIGHT - 1U) - y;
        break;
    case TFT_ROTATION_270:
        physical_x = y;
        physical_y = (int16_t)(TFT_PHYSICAL_HEIGHT - 1U) - x;
        break;
    case TFT_ROTATION_0:
    default:
        physical_x = x;
        physical_y = y;
        break;
    }

    /* 页式布局：同一列的连续 8 行存放在一个字节中。 */
    index = (uint16_t)physical_x + ((uint16_t)physical_y >> 3U) * TFT_PHYSICAL_WIDTH;
    mask = (uint8_t)(1U << ((uint16_t)physical_y & 7U));
    value = &tft_buffers[tft_draw_index][index];

    /* 前景像素服从 SET/CLEAR/XOR；实心背景使用 SET/CLEAR 的反操作。 */
    if (source_pixel) {
        if (tft_draw_mode == TFT_DRAW_SET) {
            *value |= mask;
        } else if (tft_draw_mode == TFT_DRAW_CLEAR) {
            *value &= (uint8_t)~mask;
        } else {
            *value ^= mask;
        }
    } else if (tft_draw_mode == TFT_DRAW_SET) {
        *value &= (uint8_t)~mask;
    } else {
        *value |= mask;
    }
}

void TFT_InternalPlot(int16_t x, int16_t y)
{
    TFT_InternalPlotSource(x, y, true);
}

uint16_t TFT_InternalGetLogicalWidth(void)
{
    return (tft_rotation == TFT_ROTATION_90 || tft_rotation == TFT_ROTATION_270)
               ? TFT_PHYSICAL_HEIGHT
               : TFT_PHYSICAL_WIDTH;
}

uint16_t TFT_InternalGetLogicalHeight(void)
{
    return (tft_rotation == TFT_ROTATION_90 || tft_rotation == TFT_ROTATION_270)
               ? TFT_PHYSICAL_WIDTH
               : TFT_PHYSICAL_HEIGHT;
}

void TFT_InternalGetClip(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1)
{
    *x0 = tft_clip_x0;
    *y0 = tft_clip_y0;
    *x1 = tft_clip_x1;
    *y1 = tft_clip_y1;
}

bool TFT_InternalIntersectClip(int32_t *x0, int32_t *y0,
                                int32_t *x1, int32_t *y1)
{
    if (*x0 < tft_clip_x0) {
        *x0 = tft_clip_x0;
    }
    if (*y0 < tft_clip_y0) {
        *y0 = tft_clip_y0;
    }
    if (*x1 > tft_clip_x1) {
        *x1 = tft_clip_x1;
    }
    if (*y1 > tft_clip_y1) {
        *y1 = tft_clip_y1;
    }
    return *x0 < *x1 && *y0 < *y1;
}

const uint8_t *TFT_InternalGetTransferBuffer(void)
{
    return tft_buffers[tft_transfer_index];
}

uint8_t TFT_InternalGetTransferMinX(uint8_t page)
{
    return (page < TFT_PHYSICAL_PAGES) ? tft_dirty_min[page] : TFT_PHYSICAL_WIDTH;
}

uint8_t TFT_InternalGetTransferMaxX(uint8_t page)
{
    return (page < TFT_PHYSICAL_PAGES) ? tft_dirty_max[page] : 0U;
}

void TFT_InternalTransferFinished(TFT_Status status)
{
    if (!tft_async_pending) {
        return;
    }

    /* 无论成功与否，冻结帧都是下一次休眠恢复时可完整重发的稳定图像。 */
    tft_stable_index = tft_transfer_index;
    tft_async_pending = false;
    tft_last_status = status;
    if (status == TFT_OK) {
        tft_force_full = false;
    } else {
        /* 异步失败后屏幕内容不可信，下一帧强制全屏恢复。 */
        tft_force_full = true;
    }
}

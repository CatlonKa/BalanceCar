#include "TFT_Driver.h"

#include "TFT_Internal.h"

#include "tim.h" /* 背光 PWM：TIM8_CH3N（PB1） */

/*
 * 本文件是固定硬件适配边界：STM32 HAL、SPI1 和 ST7735S 的全部参数都集中在此。
 *
 * ST7735S 是 RGB565 彩色控制器，而图形核心使用 1-bit 页式显存：
 * 每个显存字节对应同一列的 8 个垂直像素，bit0 位于该页最上方。
 * 因此驱动在发送前把每个字节展开成 8 个 RGB565 像素再写入控制器，
 * 图形核心和公共接口都不需要为此改动。
 */

/* ------------------------------------------------------------ 命令定义 ---- */

#define TFT_CMD_SWRESET 0x01U /**< 软件复位。 */
#define TFT_CMD_SLPIN   0x10U /**< 进入睡眠模式。 */
#define TFT_CMD_SLPOUT  0x11U /**< 退出睡眠模式。 */
#define TFT_CMD_NORON   0x13U /**< 进入正常显示模式。 */
#define TFT_CMD_INVOFF  0x20U /**< 关闭显示反显。 */
#define TFT_CMD_DISPON  0x29U /**< 打开显示。 */
#define TFT_CMD_CASET   0x2AU /**< 设置列地址窗口。 */
#define TFT_CMD_RASET   0x2BU /**< 设置行地址窗口。 */
#define TFT_CMD_RAMWR   0x2CU /**< 开始写入显存。 */
#define TFT_CMD_MADCTL  0x36U /**< 显存访问方向控制。 */
#define TFT_CMD_COLMOD  0x3AU /**< 像素格式设置。 */
#define TFT_CMD_FRMCTR1 0xB1U /**< 正常模式帧率控制。 */
#define TFT_CMD_FRMCTR2 0xB2U /**< 空闲模式帧率控制。 */
#define TFT_CMD_FRMCTR3 0xB3U /**< 部分模式帧率控制。 */
#define TFT_CMD_INVCTR  0xB4U /**< 显示反显控制。 */
#define TFT_CMD_PWCTR1  0xC0U /**< 电源控制 1。 */
#define TFT_CMD_PWCTR2  0xC1U /**< 电源控制 2。 */
#define TFT_CMD_PWCTR3  0xC2U /**< 电源控制 3。 */
#define TFT_CMD_PWCTR4  0xC3U /**< 电源控制 4。 */
#define TFT_CMD_PWCTR5  0xC4U /**< 电源控制 5。 */
#define TFT_CMD_VMCTR1  0xC5U /**< VCOM 控制 1。 */
#define TFT_CMD_GMCTRP1 0xE0U /**< 正极性 Gamma 校正。 */
#define TFT_CMD_GMCTRN1 0xE1U /**< 负极性 Gamma 校正。 */

/** 整页展开成 RGB565 后的字节数：整宽 x 8 行 x 2 字节。 */
#define TFT_PAGE_PIXEL_BYTES (TFT_PHYSICAL_WIDTH * 8U * 2U)

/** 单个脏页需要的传输段数：CASET 命令、CASET 参数、RASET 命令、
 *  RASET 参数、RAMWR 命令、页数据。 */
#define TFT_SEGMENT_CAPACITY 6U

/** 初始化表内单条命令允许的最大参数个数。 */
#define TFT_INIT_PARAMETER_CAPACITY 16U

/* ---------------------------------------------------------------- 类型 ---- */

/** 当前异步刷新采用的数据发送方式。 */
typedef enum {
    TFT_DRIVER_MODE_NONE = 0, /**< 没有异步任务，或正在阻塞调用。 */
    TFT_DRIVER_MODE_IT,       /**< 所有段都用中断发送。 */
    TFT_DRIVER_MODE_DMA       /**< 命令段用中断，页数据段用 DMA。 */
} TFT_DriverMode;

/** 一次 SPI 传输及其直流电平。 */
typedef struct {
    const uint8_t *data; /**< 传输源地址。 */
    uint16_t length;     /**< 传输字节数。 */
    bool is_data;        /**< true 表示 DC 为数据电平，false 为命令电平。 */
    bool use_dma;        /**< true 表示 DMA 模式下该段改用 DMA 发送。 */
} TFT_DriverSegment;

/** 初始化表中的一条命令。 */
typedef struct {
    uint8_t command;                                  /**< 命令字节。 */
    uint8_t length;                                   /**< 参数个数。 */
    uint16_t delay_ms;                                /**< 发送后需要等待的时间。 */
    uint8_t parameters[TFT_INIT_PARAMETER_CAPACITY];  /**< 参数字节。 */
} TFT_InitEntry;

/* ------------------------------------------------------------ 模块状态 ---- */

static volatile bool tft_driver_busy;              /**< 驱动状态机是否占用总线。 */
static volatile TFT_DriverMode tft_driver_mode;    /**< 当前 IT 或 DMA 模式。 */
static uint8_t tft_driver_page;                    /**< 当前正在发送的物理页。 */
static uint8_t tft_driver_segment_index;           /**< 当前段的序号。 */
static uint8_t tft_driver_segment_count;           /**< 当前页的段总数。 */

/** CASET 寻址命令：命令字节加四个参数字节。 */
static uint8_t tft_driver_column_command[5];
/** RASET 寻址命令：命令字节加四个参数字节。 */
static uint8_t tft_driver_row_command[5];
/** RAMWR 命令字节，单独存在以保持 const 属性。 */
static const uint8_t tft_driver_ramwr_command = TFT_CMD_RAMWR;

/** 当前页的传输段描述表。 */
static TFT_DriverSegment tft_driver_segments[TFT_SEGMENT_CAPACITY];
/** 一个页字节展开成 RGB565 后的像素流，也是 DMA 的源缓冲区。 */
static uint8_t tft_driver_page_pixels[TFT_PAGE_PIXEL_BYTES];

/*
 * 上电初始化表。ST7735S 的电源、帧率、Gamma 参数相互关联，
 * 这里按 ST7735S 数据手册推荐的 128x160 模组配置给出。
 * SWRESET、SLPOUT、DISPON 因为等待时间较长，在代码中单独发送。
 */
static const TFT_InitEntry tft_init_sequence[] = {
    {TFT_CMD_FRMCTR1, 3U, 0U, {0x01U, 0x2CU, 0x2DU}},
    {TFT_CMD_FRMCTR2, 3U, 0U, {0x01U, 0x2CU, 0x2DU}},
    {TFT_CMD_FRMCTR3, 6U, 0U, {0x01U, 0x2CU, 0x2DU, 0x01U, 0x2CU, 0x2DU}},
    {TFT_CMD_INVCTR,  1U, 0U, {0x07U}},
    {TFT_CMD_PWCTR1,  3U, 0U, {0xA2U, 0x02U, 0x84U}},
    {TFT_CMD_PWCTR2,  1U, 0U, {0xC5U}},
    {TFT_CMD_PWCTR3,  2U, 0U, {0x0AU, 0x00U}},
    {TFT_CMD_PWCTR4,  2U, 0U, {0x8AU, 0x2AU}},
    {TFT_CMD_PWCTR5,  2U, 0U, {0x8AU, 0xEEU}},
    {TFT_CMD_VMCTR1,  1U, 0U, {0x0EU}},
    {TFT_CMD_INVOFF,  0U, 0U, {0U}},
    {TFT_CMD_MADCTL,  1U, 0U, {TFT_MADCTL_VALUE}},
    {TFT_CMD_COLMOD,  1U, 0U, {TFT_COLMOD_VALUE}},
    {TFT_CMD_GMCTRP1, 16U, 0U, {0x02U, 0x1CU, 0x07U, 0x12U, 0x37U, 0x32U, 0x29U, 0x2DU,
                                0x29U, 0x25U, 0x2BU, 0x39U, 0x00U, 0x01U, 0x03U, 0x10U}},
    {TFT_CMD_GMCTRN1, 16U, 0U, {0x03U, 0x1DU, 0x07U, 0x06U, 0x2EU, 0x2CU, 0x29U, 0x2DU,
                                0x2EU, 0x2EU, 0x37U, 0x3FU, 0x00U, 0x00U, 0x02U, 0x10U}},
    {TFT_CMD_NORON,   0U, 10U, {0U}}
};

/* ------------------------------------------------------------ 底层操作 ---- */

/** 将 HAL 状态转换为库的统一状态。 */
static TFT_Status tft_map_hal_status(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) {
        return TFT_OK;
    }
    if (status == HAL_BUSY) {
        return TFT_BUSY;
    }
    return TFT_ERROR;
}

/** 片选拉低；一帧传输期间保持有效，避免控制器接口状态被复位。 */
static void tft_select(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
}

/** 片选拉高，结束一帧传输。 */
static void tft_deselect(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/** 设置直流电平：false 表示后续字节是命令，true 表示是数据。 */
static void tft_set_dc(bool is_data)
{
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin,
                      is_data ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * 设置背光亮度，level 为 0~100 的百分比。
 *
 * PB1 已经改成 TIM8_CH3N 的 PWM 输出，所以这里不再是写 GPIO 电平，
 * 而是改比较值：
 *   TIM8 时钟 = 168 MHz（APB2），Prescaler = 420−1、Period = 100−1
 *   -> PWM 频率 = 168 MHz / 420 / 100 = 4 kHz，占空比 100 级
 *   4 kHz 远高于人眼闪烁感知，也不会像低频那样在背光电路上啸叫。
 *
 * 注意 CubeMX 把 OCNPolarity 配的是 LOW，CH3N 相对 OCxREF 是反相的：
 *   引脚高电平占比 = (Period − CCR) / Period
 * 所以「越亮」对应 CCR 越小。这一层反相已经在下面换算掉了，
 * 调用方只需要给直觉上的 0~100。
 */
static void tft_backlight_write_level(uint8_t level)
{
    uint32_t period = (uint32_t)htim8.Init.Period + 1U;
    uint32_t compare;

    if (level > (uint8_t)TFT_BACKLIGHT_LEVEL_MAX) {
        level = (uint8_t)TFT_BACKLIGHT_LEVEL_MAX;
    }

#if TFT_BACKLIGHT_ACTIVE_HIGH
    /* level=100 -> CCR=0，引脚恒高（最亮）；level=0 -> CCR=period，引脚恒低（熄灭）。 */
    compare = period -
              (period * (uint32_t)level) / (uint32_t)TFT_BACKLIGHT_LEVEL_MAX;
#else
    compare = (period * (uint32_t)level) / (uint32_t)TFT_BACKLIGHT_LEVEL_MAX;
#endif

    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, compare);
}

/** 开关背光，等价于把亮度设成最亮或最暗。 */
static void tft_backlight_write(bool enable)
{
    tft_backlight_write_level(enable ? (uint8_t)TFT_BACKLIGHT_LEVEL_MAX : 0U);
}

/** 启动背光 PWM。 */
static void tft_backlight_start(void)
{
    /*
     * 背光用的是互补输出 CH3N，必须用 PWMN 版本启动：
     * HAL_TIM_PWM_Start() 只开 CC3E（主通道，PB1 没引出），
     * 而 HAL_TIMEx_PWMN_Start() 会开 CC3NE 并置位 MOE 主输出使能。
     * 缺了 MOE，引脚不会输出任何波形。
     */
    (void)HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_3);
}

/** 阻塞发送一段命令或数据；片选由调用方保持。 */
static HAL_StatusTypeDef tft_transmit_blocking(bool is_data, const uint8_t *data,
                                               uint16_t length)
{
    tft_set_dc(is_data);
    return HAL_SPI_Transmit(&TFT_SPI_HANDLE, data, length, TFT_BLOCKING_TIMEOUT_MS);
}

/** 阻塞发送一条带参数的命令。 */
static HAL_StatusTypeDef tft_write_command_blocking(uint8_t command,
                                                    const uint8_t *parameters,
                                                    uint16_t length)
{
    HAL_StatusTypeDef status = tft_transmit_blocking(false, &command, 1U);

    if (status == HAL_OK && length > 0U) {
        status = tft_transmit_blocking(true, parameters, length);
    }
    return status;
}

/** 阻塞发送一条无参数命令。 */
static HAL_StatusTypeDef tft_write_simple_command_blocking(uint8_t command)
{
    tft_set_dc(false);
    return HAL_SPI_Transmit(&TFT_SPI_HANDLE, &command, 1U, TFT_BLOCKING_TIMEOUT_MS);
}

/** 硬件复位；ST7735S 的 RESX 为低电平有效。 */
static void tft_hardware_reset(void)
{
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(120U);
}

/* -------------------------------------------------------------- 寻址 ---- */

/**
 * 填充 CASET / RASET 命令缓冲区。
 * 传入的是控制器显存坐标，已经包含模组的列偏移和行偏移。
 */
static void tft_build_window(uint16_t start_x, uint16_t end_x,
                             uint16_t start_y, uint16_t end_y)
{
    tft_driver_column_command[0] = TFT_CMD_CASET;
    tft_driver_column_command[1] = (uint8_t)(start_x >> 8U);
    tft_driver_column_command[2] = (uint8_t)(start_x & 0xFFU);
    tft_driver_column_command[3] = (uint8_t)(end_x >> 8U);
    tft_driver_column_command[4] = (uint8_t)(end_x & 0xFFU);

    tft_driver_row_command[0] = TFT_CMD_RASET;
    tft_driver_row_command[1] = (uint8_t)(start_y >> 8U);
    tft_driver_row_command[2] = (uint8_t)(start_y & 0xFFU);
    tft_driver_row_command[3] = (uint8_t)(end_y >> 8U);
    tft_driver_row_command[4] = (uint8_t)(end_y & 0xFFU);
}

/* -------------------------------------------------------------- 展开 ---- */

/**
 * 把一页中从 min_x 到 max_x 的显存字节展开成 RGB565 像素流。
 * 显存按列连续存放，字节内 bit0 是该页最上面一行，而控制器按行优先接收像素，
 * 所以这里外层遍历行、内层遍历列。
 * 返回展开后的字节数。
 */
static uint16_t tft_expand_page(uint8_t page, uint8_t min_x, uint8_t max_x)
{
    const uint8_t *buffer = TFT_InternalGetTransferBuffer();
    const uint8_t *source = buffer + (uint16_t)page * TFT_PHYSICAL_WIDTH + min_x;
    uint16_t columns = (uint16_t)(max_x - min_x) + 1U;
    uint16_t index = 0U;
    uint8_t row;

    for (row = 0U; row < 8U; ++row) {
        uint8_t mask = (uint8_t)(1U << row);
        uint16_t column;

        for (column = 0U; column < columns; ++column) {
            uint16_t color = ((source[column] & mask) != 0U)
                                 ? (uint16_t)TFT_COLOR_FOREGROUND
                                 : (uint16_t)TFT_COLOR_BACKGROUND;

            tft_driver_page_pixels[index] = (uint8_t)(color >> 8U);
            tft_driver_page_pixels[index + 1U] = (uint8_t)(color & 0xFFU);
            index += 2U;
        }
    }
    return index;
}

/** 把整页像素流填成背景色，供初始化清屏使用。 */
static void tft_fill_background_pixels(void)
{
    uint16_t index;

    for (index = 0U; index < TFT_PAGE_PIXEL_BYTES; index += 2U) {
        tft_driver_page_pixels[index] = (uint8_t)(TFT_COLOR_BACKGROUND >> 8U);
        tft_driver_page_pixels[index + 1U] = (uint8_t)(TFT_COLOR_BACKGROUND & 0xFFU);
    }
}

/* ---------------------------------------------------------- 阻塞传输 ---- */

/** 阻塞地把一段像素流写进指定的显存窗口。 */
static HAL_StatusTypeDef tft_write_window_blocking(uint16_t start_x, uint16_t end_x,
                                                   uint16_t start_y, uint16_t end_y,
                                                   const uint8_t *pixels,
                                                   uint16_t length)
{
    tft_build_window(start_x, end_x, start_y, end_y);

    if (tft_transmit_blocking(false, &tft_driver_column_command[0], 1U) != HAL_OK ||
        tft_transmit_blocking(true, &tft_driver_column_command[1], 4U) != HAL_OK ||
        tft_transmit_blocking(false, &tft_driver_row_command[0], 1U) != HAL_OK ||
        tft_transmit_blocking(true, &tft_driver_row_command[1], 4U) != HAL_OK ||
        tft_transmit_blocking(false, &tft_driver_ramwr_command, 1U) != HAL_OK) {
        return HAL_ERROR;
    }
    return tft_transmit_blocking(true, pixels, length);
}

/** 阻塞发送一个页的差异区间。 */
static bool tft_write_page_blocking(uint8_t page)
{
    uint8_t min_x = TFT_InternalGetTransferMinX(page);
    uint8_t max_x = TFT_InternalGetTransferMaxX(page);
    uint16_t length = tft_expand_page(page, min_x, max_x);

    return tft_write_window_blocking((uint16_t)min_x + TFT_GRAM_COLUMN_OFFSET,
                                     (uint16_t)max_x + TFT_GRAM_COLUMN_OFFSET,
                                     (uint16_t)page * 8U + TFT_GRAM_ROW_OFFSET,
                                     (uint16_t)page * 8U + 7U + TFT_GRAM_ROW_OFFSET,
                                     tft_driver_page_pixels, length) == HAL_OK;
}

/** 用背景色清空整个可见区，初始化阶段使用。 */
static HAL_StatusTypeDef tft_clear_screen_blocking(void)
{
    uint8_t page;

    tft_fill_background_pixels();
    for (page = 0U; page < TFT_PHYSICAL_PAGES; ++page) {
        if (tft_write_window_blocking(TFT_GRAM_COLUMN_OFFSET,
                                      TFT_GRAM_COLUMN_OFFSET + TFT_PHYSICAL_WIDTH - 1U,
                                      (uint16_t)page * 8U + TFT_GRAM_ROW_OFFSET,
                                      (uint16_t)page * 8U + 7U + TFT_GRAM_ROW_OFFSET,
                                      tft_driver_page_pixels,
                                      TFT_PAGE_PIXEL_BYTES) != HAL_OK) {
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}

/* ---------------------------------------------------------- 异步传输 ---- */

/** 从 first 开始寻找下一个存在差异区间的物理页。 */
static bool tft_find_next_dirty_page(uint8_t first, uint8_t *page)
{
    uint8_t candidate;

    for (candidate = first; candidate < TFT_PHYSICAL_PAGES; ++candidate) {
        if (TFT_InternalGetTransferMinX(candidate) < TFT_PHYSICAL_WIDTH) {
            *page = candidate;
            return true;
        }
    }
    return false;
}

/**
 * 为一个脏页准备好传输段。
 * CASET 和 RASET 的命令字节与参数字节直流电平不同，必须拆成两段，
 * 所以一个页共六段：CASET 命令、CASET 参数、RASET 命令、RASET 参数、
 * RAMWR 命令、页数据。
 */
static void tft_prepare_page_transfer(uint8_t page)
{
    uint8_t min_x = TFT_InternalGetTransferMinX(page);
    uint8_t max_x = TFT_InternalGetTransferMaxX(page);
    uint16_t length = tft_expand_page(page, min_x, max_x);
    TFT_DriverSegment *segment = tft_driver_segments;
    uint8_t count = 0U;

    tft_build_window((uint16_t)min_x + TFT_GRAM_COLUMN_OFFSET,
                     (uint16_t)max_x + TFT_GRAM_COLUMN_OFFSET,
                     (uint16_t)page * 8U + TFT_GRAM_ROW_OFFSET,
                     (uint16_t)page * 8U + 7U + TFT_GRAM_ROW_OFFSET);

    segment[count].data = &tft_driver_column_command[0];
    segment[count].length = 1U;
    segment[count].is_data = false;
    segment[count].use_dma = false;
    ++count;

    segment[count].data = &tft_driver_column_command[1];
    segment[count].length = 4U;
    segment[count].is_data = true;
    segment[count].use_dma = false;
    ++count;

    segment[count].data = &tft_driver_row_command[0];
    segment[count].length = 1U;
    segment[count].is_data = false;
    segment[count].use_dma = false;
    ++count;

    segment[count].data = &tft_driver_row_command[1];
    segment[count].length = 4U;
    segment[count].is_data = true;
    segment[count].use_dma = false;
    ++count;

    segment[count].data = &tft_driver_ramwr_command;
    segment[count].length = 1U;
    segment[count].is_data = false;
    segment[count].use_dma = false;
    ++count;

    segment[count].data = tft_driver_page_pixels;
    segment[count].length = length;
    segment[count].is_data = true;
    segment[count].use_dma = true;
    ++count;

    tft_driver_segment_count = count;
    tft_driver_segment_index = 0U;
}

/** 启动当前段的异步传输；DMA 模式只对页数据段使用 DMA。 */
static HAL_StatusTypeDef tft_start_segment(void)
{
    const TFT_DriverSegment *segment = &tft_driver_segments[tft_driver_segment_index];

    tft_set_dc(segment->is_data);
    if (tft_driver_mode == TFT_DRIVER_MODE_DMA && segment->use_dma) {
        return HAL_SPI_Transmit_DMA(&TFT_SPI_HANDLE, segment->data, segment->length);
    }
    return HAL_SPI_Transmit_IT(&TFT_SPI_HANDLE, segment->data, segment->length);
}

/** 初始化异步状态机并启动第一个脏页的第一段。 */
static TFT_Status tft_start_async(TFT_DriverMode mode)
{
    HAL_StatusTypeDef hal_status;
    uint8_t first_page;

    if (tft_driver_busy) {
        return TFT_BUSY;
    }
    if (!tft_find_next_dirty_page(0U, &first_page)) {
        /* 没有任何差异页时不占用总线，也不产生完成事件。 */
        return TFT_OK;
    }

    /* 状态必须在调用 HAL 前写好，完成回调才能正确识别当前阶段。 */
    tft_driver_busy = true;
    tft_driver_page = first_page;
    tft_driver_mode = mode;

    tft_select();
    tft_prepare_page_transfer(first_page);
    hal_status = tft_start_segment();
    if (hal_status != HAL_OK) {
        tft_deselect();
        tft_driver_busy = false;
        tft_driver_mode = TFT_DRIVER_MODE_NONE;
        return tft_map_hal_status(hal_status);
    }
    return TFT_OK;
}

/** 释放异步状态并把最终结果交回图形核心。 */
static void tft_finish_async(TFT_Status status)
{
    /* 先结束帧传输释放片选，再通知核心完成缓冲区状态收尾。 */
    tft_deselect();
    tft_driver_busy = false;
    tft_driver_mode = TFT_DRIVER_MODE_NONE;
    TFT_InternalTransferFinished(status);
}

/* ------------------------------------------------------------ 对外接口 ---- */

/**
 * 丢弃驱动状态：屏幕已经断电或被拔掉，这里只清理本地状态。
 *
 * 刻意不做两件事：
 *   不向 SPI 发任何命令（ST7735S 已经没电，命令送不到）
 *   不等异步传输结束（主机发数据不依赖从机，等它没有意义）
 * 但背光 PWM 必须停：屏幕都不在，继续输出只是在空转。
 * 还留在路上的数据不会送到任何地方，其完成回调只会再写一遍下面的静态状态，
 * 不会越界，所以强行复位是安全的。
 */
void TFT_DriverDeInit(void)
{
    (void)HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_3);
    tft_backlight_write(false);
    tft_deselect();

    tft_driver_busy = false;
    tft_driver_mode = TFT_DRIVER_MODE_NONE;
    tft_driver_page = 0U;
    tft_driver_segment_index = 0U;
    tft_driver_segment_count = 0U;
}

/**
 * 等待面板上电稳定，发送 ST7735S 初始化序列，用背景色清空可见区，
 * 最后打开显示并点亮背光。初始化阶段故意使用阻塞调用，保证返回时屏幕状态确定。
 */
TFT_Status TFT_DriverInit(void)
{
    uint8_t index;
    HAL_StatusTypeDef hal_status;

    if (tft_driver_busy) {
        return TFT_BUSY;
    }
    tft_driver_busy = true;
    tft_driver_mode = TFT_DRIVER_MODE_NONE;
    tft_driver_segment_count = 0U;
    tft_driver_segment_index = 0U;

    /* 初始化期间先关背光，避免用户看到上电随机内容。 */
    tft_backlight_start();
    tft_backlight_write(false);
    HAL_Delay(10U);
    tft_hardware_reset();

    tft_select();

    /* 复位和退出睡眠的等待时间较长，单独发送。 */
    hal_status = tft_write_simple_command_blocking(TFT_CMD_SWRESET);
    HAL_Delay(150U);
    if (hal_status == HAL_OK) {
        hal_status = tft_write_simple_command_blocking(TFT_CMD_SLPOUT);
        HAL_Delay(120U);
    }

    for (index = 0U; hal_status == HAL_OK && index < (uint8_t)(sizeof(tft_init_sequence) /
                                                              sizeof(tft_init_sequence[0]));
         ++index) {
        const TFT_InitEntry *entry = &tft_init_sequence[index];

        hal_status = tft_write_command_blocking(entry->command, entry->parameters,
                                               entry->length);
        if (hal_status == HAL_OK && entry->delay_ms > 0U) {
            HAL_Delay(entry->delay_ms);
        }
    }

    /* 全部参数写入成功后才清屏，再打开显示，避免上电随机画面。 */
    if (hal_status == HAL_OK) {
        hal_status = tft_clear_screen_blocking();
    }
    if (hal_status == HAL_OK) {
        hal_status = tft_write_simple_command_blocking(TFT_CMD_DISPON);
        HAL_Delay(100U);
    }

    tft_deselect();
    tft_driver_busy = false;

    if (hal_status != HAL_OK) {
        return tft_map_hal_status(hal_status);
    }
    tft_backlight_write(true);
    return TFT_OK;
}

/** 按页阻塞发送核心生成的连续差异区间。 */
TFT_Status TFT_DriverWriteBlocking(void)
{
    uint8_t page;

    if (tft_driver_busy) {
        return TFT_BUSY;
    }
    tft_driver_busy = true;
    tft_driver_mode = TFT_DRIVER_MODE_NONE;

    tft_select();
    for (page = 0U; page < TFT_PHYSICAL_PAGES; ++page) {
        if (TFT_InternalGetTransferMinX(page) >= TFT_PHYSICAL_WIDTH) {
            continue;
        }
        if (!tft_write_page_blocking(page)) {
            tft_deselect();
            tft_driver_busy = false;
            return TFT_ERROR;
        }
    }
    tft_deselect();
    tft_driver_busy = false;
    return TFT_OK;
}

TFT_Status TFT_DriverWriteIT(void)
{
    return tft_start_async(TFT_DRIVER_MODE_IT);
}

TFT_Status TFT_DriverWriteDMA(void)
{
    return tft_start_async(TFT_DRIVER_MODE_DMA);
}

bool TFT_DriverIsBusy(void)
{
    return tft_driver_busy;
}

TFT_Status TFT_DriverSetContrast(uint8_t value)
{
    /* ST7735S 没有对比度寄存器，亮度只能通过背光或 Gamma 曲线调整。 */
    (void)value;
    return TFT_UNSUPPORTED;
}

TFT_Status TFT_DriverSetPowerSave(bool enable)
{
    HAL_StatusTypeDef hal_status;

    if (tft_driver_busy) {
        return TFT_BUSY;
    }
    tft_driver_busy = true;
    tft_driver_mode = TFT_DRIVER_MODE_NONE;
    tft_select();

    if (enable) {
        hal_status = tft_write_simple_command_blocking(TFT_CMD_SLPIN);
        tft_deselect();
        tft_driver_busy = false;
        if (hal_status == HAL_OK) {
            tft_backlight_write(false);
        }
        return tft_map_hal_status(hal_status);
    }

    /* 唤醒需要先退出睡眠，等面板稳定后再打开显示。 */
    hal_status = tft_write_simple_command_blocking(TFT_CMD_SLPOUT);
    if (hal_status == HAL_OK) {
        HAL_Delay(120U);
        hal_status = tft_write_simple_command_blocking(TFT_CMD_DISPON);
    }
    tft_deselect();
    tft_driver_busy = false;

    if (hal_status != HAL_OK) {
        return tft_map_hal_status(hal_status);
    }
    tft_backlight_write(true);
    return TFT_OK;
}

TFT_Status TFT_DriverSetBacklight(bool enable)
{
    tft_backlight_write(enable);
    return TFT_OK;
}

TFT_Status TFT_DriverSetBacklightLevel(uint8_t level)
{
    tft_backlight_write_level(level);
    return TFT_OK;
}

/**
 * HAL 发送完成入口：推进到下一段，本页发完则切换到下一个脏页。
 * DMA 模式只改变页数据段，寻址命令始终由中断发送。
 */
void TFT_DriverHandleTxComplete(void)
{
    HAL_StatusTypeDef hal_status;

    if (!tft_driver_busy || tft_driver_mode == TFT_DRIVER_MODE_NONE) {
        return;
    }

    if (tft_driver_segment_index + 1U < tft_driver_segment_count) {
        ++tft_driver_segment_index;
        hal_status = tft_start_segment();
        if (hal_status != HAL_OK) {
            tft_finish_async(TFT_ERROR);
        }
        return;
    }

    {
        /* 本页已经发完：没有后续脏页就结束，否则准备下一页。 */
        uint8_t next_page;

        if (!tft_find_next_dirty_page((uint8_t)(tft_driver_page + 1U), &next_page)) {
            tft_finish_async(TFT_OK);
            return;
        }
        tft_driver_page = next_page;
    }
    tft_prepare_page_transfer(tft_driver_page);
    if (tft_start_segment() != HAL_OK) {
        tft_finish_async(TFT_ERROR);
    }
}

/** HAL 错误入口，仅终止真正处于异步模式的传输。 */
void TFT_DriverHandleError(void)
{
    if (tft_driver_busy && tft_driver_mode != TFT_DRIVER_MODE_NONE) {
        tft_finish_async(TFT_ERROR);
    }
}

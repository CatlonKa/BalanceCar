#include "TFT_BootSplash.h"

#include <stdbool.h>

/*
 * 开机图的显示逻辑。图像数据在 TFT_BootSplashData.c（脚本生成），
 * 两个文件分开是为了重新生成数据时不会覆盖这里的代码。
 *
 * 实现只有一步：把选中的那张直接推给面板，然后按住一会儿。走的是
 * TFT_DirectBlit()，所以这是**彩色**的，和界面那套 1bpp 黑白显存无关。
 */

/*
 * 游标：下一次该显示哪一张。
 *
 * ⚠️ 现在只存在 RAM 里，所以掉电会归零，「每次上电换一张」尚未成立。
 * 要跨掉电轮换，只要把下面 splash_index_load/save 两个函数改成读写
 * 非易失存储即可，其余逻辑一行都不用动：
 *
 *   片内 Flash（推荐，但工作量最大）
 *     在末尾扇区做「不擦除、只顺序追加」的小日志：先扫出第一个 0xFFFFFFFF
 *     的位置，往上写一个字；整扇区写满才擦一次（32 K 次上电才擦一次，
 *     而 128 KB 扇区擦一次约 1 秒，所以均摊成本几乎为零）。
 *     代价：要在链接脚本里预留那一段，而 CubeMX 重新生成 .ld 时会丢，
 *     需要重新加上。
 *
 *   备份 SRAM（写起来最简单）
 *     开 PWR 备份域时钟，直接读写 0x40024000 开始的 4 KB。
 *     但它靠 VBAT 供电才能撑过掉电；板上 VBAT 要是只接到 3.3 V，
 *     拔电就丢了，效果与现在的 RAM 游标一样。
 */
static uint8_t s_next_index;

/** 读出跨掉电保存的游标；现在恒为 0（还没接非易失存储）。 */
static uint8_t splash_index_load(void)
{
    return 0U;
}

/** 保存跨掉电游标；现在什么都不做（还没接非易失存储）。 */
static void splash_index_save(uint8_t index)
{
    (void)index;
}

uint8_t TFT_BootSplashPickIndex(void)
{
    /* 第一次调用时从非易失存储读入初值。 */
    static bool loaded = false;
    uint8_t index;

    if (!loaded)
    {
        loaded = true;
        s_next_index = splash_index_load();
    }

    /* 数据或存储被改坏时夹回范围，不让它跑到表外。 */
    index = (s_next_index < (uint8_t)TFT_BOOT_SPLASH_COUNT) ? s_next_index : 0U;

    s_next_index = (uint8_t)(index + 1U);
    if (s_next_index >= (uint8_t)TFT_BOOT_SPLASH_COUNT)
    {
        s_next_index = 0U;
    }
    splash_index_save(s_next_index);

    return index;
}

TFT_Status TFT_BootSplashShow(uint8_t index)
{
    TFT_Status status;

    if (index >= (uint8_t)TFT_BOOT_SPLASH_COUNT)
    {
        index = 0U;
    }

    /*
     * 整屏一次写完。图像数据本来就是面板线上字节顺序（RGB565、高字节在前），
     * 所以这里没有色彩转换，数据也是从 Flash 直接段发给 SPI，不占 RAM。
     *
     * 尺寸用面板宏而不是数据表里的数字：TFT_BootSplash.h 里有一条编译期
     * 断言保证两边的字节数一致，所以这里直接按面板尺寸发是安全的。
     */
    status = TFT_DirectBlit(0, 0, TFT_PHYSICAL_WIDTH, TFT_PHYSICAL_HEIGHT,
                            tft_boot_splash_table[index]);

    /*
     * 只画，不停留：停留交给调用方用主循环计时（见 TFT_BOOT_SPLASH_HOLD_MS
     * 的说明）。这里若 HAL_Delay() 死等，主循环会被堵住，电压采样和串口都停。
     */
    return status;
}

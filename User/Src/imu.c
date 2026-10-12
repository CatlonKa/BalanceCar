#include "imu.h"
#include "spi.h"

/* SPI 等待超时，单位 ms；仅限非中断上下文调用，ISR 内 tick 冻结会让它失效 */
#define IMU_SPI_TIMEOUT      10U

/* WHO_AM_I 重试次数 / 间隔 ms */
#define IMU_INIT_RETRY       5
#define IMU_INIT_RETRY_MS     2U



/* SPI3 直接寄存器访问：不经过 HAL，省掉多层参数检查与状态机 */

/* 等 SR 的 flag 置位；超时返回 false */
static bool imu_wait_set(uint32_t flag, uint32_t tickstart)
{
    while ((SPI3->SR & flag) == 0U)
    {
        if ((HAL_GetTick() - tickstart) >= IMU_SPI_TIMEOUT)
        {
            return false;
        }
    }
    return true;
}

/* 等最后一字节真正移完，否则提前抬 CS 会被截断 */
static bool imu_wait_idle(void)
{
    uint32_t tickstart = HAL_GetTick();

    while ((SPI3->SR & SPI_SR_BSY) != 0U)
    {
        if ((HAL_GetTick() - tickstart) >= IMU_SPI_TIMEOUT)
        {
            return false;
        }
    }
    return true;
}

/* 全双工收发 len 字节；tx 为 NULL 发 0xFF，rx 为 NULL 丢弃收到的字节 */
static bool imu_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint32_t tickstart = HAL_GetTick();
    uint16_t i;

    for (i = 0U; i < len; i++)
    {
        if (!imu_wait_set(SPI_SR_TXE, tickstart))
        {
            return false;
        }
        *(__IO uint8_t *)&SPI3->DR = (tx != NULL) ? tx[i] : 0xFFU;

        if (!imu_wait_set(SPI_SR_RXNE, tickstart))
        {
            return false;
        }
        {
            uint8_t v = *(__IO uint8_t *)&SPI3->DR;
            if (rx != NULL)
            {
                rx[i] = v;
            }
        }
    }
    return imu_wait_idle();
}

/* 写 len 字节：cmd = reg（bit7=0），多字节靠 CTRL3_C.IF_INC 自增 */
static bool imu_write_regs(uint8_t reg, const uint8_t *buf, uint16_t len)
{
    bool ok;

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    ok = imu_xfer(&reg, NULL, 1U) && imu_xfer(buf, NULL, len);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    return ok;
}

/* 读 len 字节：cmd = reg | 0x80（bit7=1 读），命令那一字节的返回是地址相垃圾 */
static bool imu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t cmd = (uint8_t)(reg | 0x80U);
    bool ok;

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    ok = imu_xfer(&cmd, NULL, 1U) && imu_xfer(NULL, buf, len);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    return ok;
}


static bool imu_read_reg(uint8_t reg, uint8_t *val)
{
    return imu_read_regs(reg, val, 1U);
}

void imu_transmit(uint8_t *data, uint16_t size) 
{
    (void)imu_xfer(data, NULL, size);
}


int imu_init(void) 
{
    uint8_t v = 0U;
    uint16_t i;

    /* HAL_SPI_Init 只清 SPE 不置位，直读寄存器前必须自己开 */
    __HAL_SPI_ENABLE(&hspi3);

    /* 连续块值表：一块一次片选写完，地址靠 IF_INC 自增（复位默认打开） */
    //完全没用到的连续块注释掉

    // static const uint8_t cfg_01[] =                      /* 0x01 ~ 0x02 */
    // {
    //     IMU_CFG_FUNC_CFG_ACCESS,    /* 0x01 */
    //     IMU_CFG_PIN_CTRL            /* 0x02 */
    // };
    static const uint8_t cfg_04[] =                      /* 0x04 ~ 0x0E，跳过保留 0x03 与只读 0x0F */
    {
        IMU_CFG_S4S_TPH_L,          /* 0x04 */
        IMU_CFG_S4S_TPH_H,          /* 0x05 */
        IMU_CFG_S4S_RR,             /* 0x06 */
        IMU_CFG_FIFO_CTRL1,         /* 0x07 */
        IMU_CFG_FIFO_CTRL2,         /* 0x08 */
        IMU_CFG_FIFO_CTRL3,         /* 0x09 */
        IMU_CFG_FIFO_CTRL4,         /* 0x0A */
        IMU_CFG_COUNTER_BDR_REG1,   /* 0x0B */
        IMU_CFG_COUNTER_BDR_REG2,   /* 0x0C */
        IMU_CFG_INT1_CTRL,          /* 0x0D */
        IMU_CFG_INT2_CTRL           /* 0x0E */
    };
    static const uint8_t cfg_10[] =                      /* 0x10 ~ 0x19 */
    {
        IMU_CFG_CTRL1_XL,           /* 0x10 */
        IMU_CFG_CTRL2_G,            /* 0x11 */
        IMU_CFG_CTRL3_C,            /* 0x12 */
        IMU_CFG_CTRL4_C,            /* 0x13 */
        IMU_CFG_CTRL5_C,            /* 0x14 */
        IMU_CFG_CTRL6_C,            /* 0x15 */
        IMU_CFG_CTRL7_G,            /* 0x16 */
        IMU_CFG_CTRL8_XL,           /* 0x17 */
        IMU_CFG_CTRL9_XL,           /* 0x18 */
        IMU_CFG_CTRL10_C            /* 0x19 */
    };
    // static const uint8_t cfg_56[] =                      /* 0x56 ~ 0x62 */
    // {
    //     IMU_CFG_TAP_CFG0,           /* 0x56 */
    //     IMU_CFG_TAP_CFG1,           /* 0x57 */
    //     IMU_CFG_TAP_CFG2,           /* 0x58 */
    //     IMU_CFG_TAP_THS_6D,         /* 0x59 */
    //     IMU_CFG_INT_DUR2,           /* 0x5A */
    //     IMU_CFG_WAKE_UP_THS,        /* 0x5B */
    //     IMU_CFG_WAKE_UP_DUR,        /* 0x5C */
    //     IMU_CFG_FREE_FALL,          /* 0x5D */
    //     IMU_CFG_MD1_CFG,            /* 0x5E */
    //     IMU_CFG_MD2_CFG,            /* 0x5F */
    //     IMU_CFG_S4S_ST_CMD_CODE,    /* 0x60 */
    //     IMU_CFG_S4S_DT_REG,         /* 0x61 */
    //     IMU_CFG_I3C_BUS_AVB         /* 0x62 */
    // };
    // static const uint8_t cfg_73[] =                      /* 0x73 ~ 0x75 */
    // {
    //     IMU_CFG_X_OFS_USR,          /* 0x73 */
    //     IMU_CFG_Y_OFS_USR,          /* 0x74 */
    //     IMU_CFG_Z_OFS_USR           /* 0x75 */
    // };

    /* 1. 等芯片应答；没检测到就重复检测 */
    for (i = 0U; i < IMU_INIT_RETRY; i++)
    {
        v = 0U;
        (void)imu_read_reg(IMU_REG_WHO_AM_I, &v);
        if (v == IMU_WHO_AM_I_VAL)
        {
            break;
        }
        HAL_Delay(IMU_INIT_RETRY_MS);
    }
    if (i >= IMU_INIT_RETRY)
    {
        return IMU_ERR_NO_DEVICE;
    }

    /* 2. 连续块写：5 次片选写完 39 个配置寄存器 */
    //imu_write_regs(IMU_REG_FUNC_CFG_ACCESS, cfg_01, (uint16_t)sizeof(cfg_01));
    (void)imu_write_regs(IMU_REG_S4S_TPH_L,       cfg_04, (uint16_t)sizeof(cfg_04));
    (void)imu_write_regs(IMU_REG_CTRL1_XL,        cfg_10, (uint16_t)sizeof(cfg_10));
    //imu_write_regs(IMU_REG_TAP_CFG0,        cfg_56, (uint16_t)sizeof(cfg_56));
    //imu_write_regs(IMU_REG_X_OFS_USR,       cfg_73, (uint16_t)sizeof(cfg_73));

    return IMU_OK;
}


/* 读陀螺 6B + 加速度 6B：从 OUTX_L_G(0x22) 起自增，BDU=1 保证同一组数据 */
bool imu_read_data(uint8_t *data, uint16_t size) 
{
    return imu_read_regs(IMU_REG_OUTX_L_G, data, size);
}

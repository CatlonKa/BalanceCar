#include "imu.h"
#include "spi.h"

/* 单次 SPI 事务超时，单位 ms；中断内 tick 冻结，超时不会触发 */
#define IMU_SPI_TIMEOUT      10U

/* WHO_AM_I 重试次数 / 间隔 ms */
#define IMU_INIT_RETRY       5
#define IMU_INIT_RETRY_MS     2U

/* 软复位后等待 ms */
#define IMU_RESET_DELAY_MS    5U

/* 写 len 字节：cmd = reg（bit7=0），多字节靠 CTRL3_C.IF_INC 自增 */
static void imu_write_regs(uint8_t reg, const uint8_t *buf, uint16_t len)
{
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi3, &reg, 1U, IMU_SPI_TIMEOUT);
    HAL_SPI_Transmit(&hspi3, (uint8_t *)buf, len, IMU_SPI_TIMEOUT);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
}

/* 读 len 字节：cmd = reg | 0x80（bit7=1 读），多字节靠 IF_INC 自增 */
static void imu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t cmd = (uint8_t)(reg | 0x80U);

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi3, &cmd, 1U, IMU_SPI_TIMEOUT);
    HAL_SPI_Receive(&hspi3, buf, len, IMU_SPI_TIMEOUT);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
}

static void imu_write_reg(uint8_t reg, uint8_t val)
{
    imu_write_regs(reg, &val, 1U);
}

static void imu_read_reg(uint8_t reg, uint8_t *val)
{
    imu_read_regs(reg, val, 1U);
}

void imu_transmit(uint8_t *data, uint16_t size) 
{
    HAL_SPI_Transmit(&hspi3, data, size, 2);
}


int imu_init(void) 
{
    uint8_t v = 0U;
    uint16_t i;

    /* 连续块值表：一块一次片选写完，地址靠 IF_INC 自增（复位默认打开） */
    static const uint8_t cfg_01[] =                      /* 0x01 ~ 0x02 */
    {
        IMU_CFG_FUNC_CFG_ACCESS,    /* 0x01 */
        IMU_CFG_PIN_CTRL            /* 0x02 */
    };
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
    static const uint8_t cfg_56[] =                      /* 0x56 ~ 0x62 */
    {
        IMU_CFG_TAP_CFG0,           /* 0x56 */
        IMU_CFG_TAP_CFG1,           /* 0x57 */
        IMU_CFG_TAP_CFG2,           /* 0x58 */
        IMU_CFG_TAP_THS_6D,         /* 0x59 */
        IMU_CFG_INT_DUR2,           /* 0x5A */
        IMU_CFG_WAKE_UP_THS,        /* 0x5B */
        IMU_CFG_WAKE_UP_DUR,        /* 0x5C */
        IMU_CFG_FREE_FALL,          /* 0x5D */
        IMU_CFG_MD1_CFG,            /* 0x5E */
        IMU_CFG_MD2_CFG,            /* 0x5F */
        IMU_CFG_S4S_ST_CMD_CODE,    /* 0x60 */
        IMU_CFG_S4S_DT_REG,         /* 0x61 */
        IMU_CFG_I3C_BUS_AVB         /* 0x62 */
    };
    static const uint8_t cfg_73[] =                      /* 0x73 ~ 0x75 */
    {
        IMU_CFG_X_OFS_USR,          /* 0x73 */
        IMU_CFG_Y_OFS_USR,          /* 0x74 */
        IMU_CFG_Z_OFS_USR           /* 0x75 */
    };

    /* 1. 等芯片应答；没检测到就重复检测 */
    for (i = 0U; i < IMU_INIT_RETRY; i++)
    {
        imu_read_reg(IMU_REG_WHO_AM_I, &v);
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

    /* 2. 软复位，等复位完成 */
    imu_write_reg(IMU_REG_CTRL3_C, 0x01U);
    HAL_Delay(IMU_RESET_DELAY_MS);

    /* 3. 连续块写：5 次片选写完 39 个配置寄存器 */
    imu_write_regs(IMU_REG_FUNC_CFG_ACCESS, cfg_01, (uint16_t)sizeof(cfg_01));
    imu_write_regs(IMU_REG_S4S_TPH_L,       cfg_04, (uint16_t)sizeof(cfg_04));
    imu_write_regs(IMU_REG_CTRL1_XL,        cfg_10, (uint16_t)sizeof(cfg_10));
    imu_write_regs(IMU_REG_TAP_CFG0,        cfg_56, (uint16_t)sizeof(cfg_56));
    imu_write_regs(IMU_REG_X_OFS_USR,       cfg_73, (uint16_t)sizeof(cfg_73));

    return IMU_OK;
}


/* 读陀螺 6B + 加速度 6B：从 OUTX_L_G(0x22) 起自增，BDU=1 保证同一组数据 */
void imu_read_data(uint8_t *data, uint16_t size) 
{
    imu_read_regs(IMU_REG_OUTX_L_G, data, size);
}

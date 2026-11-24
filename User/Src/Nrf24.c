#include "Nrf24.h"
#include "main.h"
#include "spi.h"

/* nRF24L01+ 命令字 */
#define NRF_R_REGISTER      0x00U
#define NRF_W_REGISTER      0x20U
#define NRF_R_RX_PAYLOAD    0x61U
#define NRF_W_TX_PAYLOAD    0xA0U
#define NRF_FLUSH_TX        0xE1U
#define NRF_FLUSH_RX        0xE2U
#define NRF_NOP             0xFFU
#define NRF_REGISTER_MASK   0x1FU

/* 寄存器地址 */
#define NRF_REG_CONFIG      0x00U
#define NRF_REG_EN_AA       0x01U
#define NRF_REG_EN_RXADDR   0x02U
#define NRF_REG_SETUP_AW    0x03U
#define NRF_REG_SETUP_RETR  0x04U
#define NRF_REG_RF_CH       0x05U
#define NRF_REG_RF_SETUP    0x06U
#define NRF_REG_STATUS      0x07U
#define NRF_REG_RX_ADDR_P0  0x0AU
#define NRF_REG_TX_ADDR     0x10U
#define NRF_REG_RX_PW_P0    0x11U

/* STATUS 位 */
#define NRF_STATUS_TX_FULL  0x01U
#define NRF_STATUS_TX_DS    0x20U
#define NRF_STATUS_MAX_RT   0x10U

/* 配置值：2 字节 CRC + PWR_UP，PRIM_RX=0 即发送模式 */
#define NRF_CONFIG_TX       0x0EU
#define NRF_SETUP_AW_5BYTES 0x03U
/* RF_SETUP：RF_PWR=11(0 dBm)、RF_DR_HIGH=0 → 1 Mbps */
#define NRF_RF_SETUP_1M_0DBM 0x06U

/* 5 字节收发地址，两端必须一致 */
static const uint8_t s_addr[5] = { 'B', 'A', 'L', 'N', '0' };

/* SPI 等待上限（软件计数，不依赖 SysTick —— 本工程 ISR 优先级都高于它） */
#define NRF_SPI_GUARD       20000U

static uint8_t s_last_status;

#define NRF_CSN_LOW()   HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET)
#define NRF_CSN_HIGH()  HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_SET)
#define NRF_CE_LOW()    HAL_GPIO_WritePin(WIFI_CE_GPIO_Port, WIFI_CE_Pin, GPIO_PIN_RESET)
#define NRF_CE_HIGH()   HAL_GPIO_WritePin(WIFI_CE_GPIO_Port, WIFI_CE_Pin, GPIO_PIN_SET)

/* 粗略忙等：168 MHz、-O0 下每轮约 20 周期，系数 8 偏保守 */
static void nrf_delay_us(uint32_t us)
{
    volatile uint32_t n = us * 8U;
    while (n != 0U)
    {
        n--;
    }
}

/* 全双工收发；tx 为 NULL 发 0xFF，rx 为 NULL 丢字节 */
static void nrf_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t i;

    for (i = 0U; i < len; i++)
    {
        uint32_t guard = NRF_SPI_GUARD;

        while (((SPI2->SR & SPI_SR_TXE) == 0U) && (--guard != 0U))
        {
        }
        *(__IO uint8_t *)&SPI2->DR = (tx != NULL) ? tx[i] : 0xFFU;

        guard = NRF_SPI_GUARD;
        while (((SPI2->SR & SPI_SR_RXNE) == 0U) && (--guard != 0U))
        {
        }
        {
            uint8_t v = *(__IO uint8_t *)&SPI2->DR;
            if (rx != NULL)
            {
                rx[i] = v;
            }
        }
    }

    /* 抬 CSN 前等最后一字节移完，否则会被截断 */
    {
        uint32_t guard = NRF_SPI_GUARD;
        while (((SPI2->SR & SPI_SR_BSY) != 0U) && (--guard != 0U))
        {
        }
    }
}

/* 单字节命令，返回 STATUS */
static uint8_t nrf_cmd(uint8_t cmd)
{
    uint8_t v = 0U;

    NRF_CSN_LOW();
    nrf_xfer(&cmd, &v, 1U);
    NRF_CSN_HIGH();
    return v;
}

static void nrf_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(NRF_W_REGISTER | (reg & NRF_REGISTER_MASK));
    buf[1] = val;

    NRF_CSN_LOW();
    nrf_xfer(buf, NULL, 2U);
    NRF_CSN_HIGH();
}

static uint8_t nrf_read_reg(uint8_t reg)
{
    uint8_t cmd = (uint8_t)(NRF_R_REGISTER | (reg & NRF_REGISTER_MASK));
    uint8_t v = 0U;

    NRF_CSN_LOW();
    nrf_xfer(&cmd, NULL, 1U);
    nrf_xfer(NULL, &v, 1U);
    NRF_CSN_HIGH();
    return v;
}

static void nrf_write_buf(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t cmd = (uint8_t)(NRF_W_REGISTER | (reg & NRF_REGISTER_MASK));

    NRF_CSN_LOW();
    nrf_xfer(&cmd, NULL, 1U);
    nrf_xfer(data, NULL, len);
    NRF_CSN_HIGH();
}

bool nrf_init(void)
{
    /* HAL_SPI_Init 只清 SPE 不置位，直接寄存器访问前必须自己开 */
    __HAL_SPI_ENABLE(&hspi2);

    NRF_CE_LOW();
    NRF_CSN_HIGH();

    /* 上电到可配置至少要 100 ms，这里取 5 ms 是因为调用点已在开机自检之后，
       模块早在 TFT 初始化的几百毫秒里上电稳定了 */
    nrf_delay_us(5000U);

    nrf_write_reg(NRF_REG_CONFIG, 0x0CU);          /* 先不 PWR_UP */
    nrf_write_reg(NRF_REG_EN_AA, 0x00U);           /* 关自动应答 */
    nrf_write_reg(NRF_REG_EN_RXADDR, 0x01U);       /* 只开 P0 */
    nrf_write_reg(NRF_REG_SETUP_AW, NRF_SETUP_AW_5BYTES);
    nrf_write_reg(NRF_REG_SETUP_RETR, 0x00U);      /* 不重发 */
    nrf_write_reg(NRF_REG_RF_CH, NRF24_RF_CHANNEL);
    nrf_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_1M_0DBM);

    nrf_write_buf(NRF_REG_TX_ADDR, s_addr, 5U);
    nrf_write_buf(NRF_REG_RX_ADDR_P0, s_addr, 5U);
    nrf_write_reg(NRF_REG_RX_PW_P0, NRF24_PAYLOAD_MAX);

    (void)nrf_cmd(NRF_FLUSH_TX);
    (void)nrf_cmd(NRF_FLUSH_RX);
    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_TX);
    nrf_delay_us(2000U);                           /* PWR_UP 到 standby 需 1.5 ms */

    s_last_status = 0U;

    /* 自检：回读信道能对上才算 SPI 通了（模块没插/没供电时恒读 0x00 或 0xFF） */
    return (nrf_read_reg(NRF_REG_RF_CH) == NRF24_RF_CHANNEL);
}

bool nrf_send(const uint8_t *data, uint8_t len)
{
    uint8_t cmd = NRF_W_TX_PAYLOAD;
    bool ok = true;

    if ((data == NULL) || (len == 0U))
    {
        return false;
    }
    if (len > NRF24_PAYLOAD_MAX)
    {
        len = NRF24_PAYLOAD_MAX;
    }

    /* 上一包还没发完就丢掉本帧，绝不等待：控制环和遥测都不能被射频拖住 */
    s_last_status = nrf_cmd(NRF_NOP);
    if ((s_last_status & NRF_STATUS_TX_FULL) != 0U)
    {
        (void)nrf_cmd(NRF_FLUSH_TX);
        ok = false;
    }

    NRF_CSN_LOW();
    nrf_xfer(&cmd, NULL, 1U);
    nrf_xfer(data, NULL, len);
    NRF_CSN_HIGH();

    /* CE 高电平 > 10 us 才触发发射 */
    NRF_CE_HIGH();
    nrf_delay_us(20U);
    NRF_CE_LOW();

    /* STATUS 写 1 清标志，防下一次误判 */
    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    return ok;
}

uint8_t nrf_last_status(void)
{
    return s_last_status;
}

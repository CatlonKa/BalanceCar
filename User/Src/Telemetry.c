#include "Telemetry.h"
#include "Nrf24.h"

#include <string.h>

/* 包布局：seq(u16) | N×float | crc16，全小端，总长 = 2 + 4N + 2 */
#define TELEMETRY_BODY      (2U + TELEMETRY_RADIO_FIELDS * 4U)
#define TELEMETRY_PACKET    (TELEMETRY_BODY + 2U)

#if (TELEMETRY_PACKET > NRF24_PAYLOAD_MAX)
#error "遥测包超过射频单包上限，减少 TELEMETRY_RADIO_FIELDS"
#endif

static uint16_t s_seq;
static uint8_t  s_packet[TELEMETRY_PACKET];

/* CRC-16/CCITT-FALSE：多项式 0x1021、初值 0xFFFF、不反转 */
static uint16_t telemetry_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t  bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void telemetry_put_f32(uint8_t *dst, float v)
{
    uint32_t bits;

    memcpy(&bits, &v, sizeof(bits));
    dst[0] = (uint8_t)bits;
    dst[1] = (uint8_t)(bits >> 8);
    dst[2] = (uint8_t)(bits >> 16);
    dst[3] = (uint8_t)(bits >> 24);
}

bool Telemetry_Init(void)
{
    s_seq = 0U;
    return nrf_init();
}

bool Telemetry_Send(const float *values, uint16_t count)
{
    uint16_t i;
    uint16_t crc;

    if (values == NULL || count == 0U)
    {
        return false;
    }
    if (count > TELEMETRY_RADIO_FIELDS)
    {
        count = TELEMETRY_RADIO_FIELDS;
    }

    s_packet[0] = (uint8_t)s_seq;
    s_packet[1] = (uint8_t)(s_seq >> 8);

    for (i = 0U; i < count; i++)
    {
        telemetry_put_f32(&s_packet[2U + 4U * i], values[i]);
    }
    /* 包长固定，未用槽位填 0：留上一帧数据会被接收端解成杂值 */
    for (i = count; i < TELEMETRY_RADIO_FIELDS; i++)
    {
        telemetry_put_f32(&s_packet[2U + 4U * i], 0.0f);
    }

    crc = telemetry_crc16(s_packet, TELEMETRY_BODY);
    s_packet[TELEMETRY_BODY] = (uint8_t)crc;
    s_packet[TELEMETRY_BODY + 1U] = (uint8_t)(crc >> 8);

    /* 上一包还没发完就丢帧，绝不等待：控制环与遥测都不能被射频拖住 */
    if (!nrf_send(s_packet, (uint8_t)TELEMETRY_PACKET))
    {
        return false;
    }
    s_seq++;
    return true;
}


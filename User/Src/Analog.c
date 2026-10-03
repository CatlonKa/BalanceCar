#include "Analog.h"

/* 每半区累计的采样轮数：8 轮 x 2 通道 = 16 次 DMA 传输 */
#define ANALOG_AVG_N       8U
#define ANALOG_CHANNELS    2U
#define ANALOG_HALF_N      (ANALOG_AVG_N * ANALOG_CHANNELS)
#define ANALOG_BUFFER_N    (ANALOG_HALF_N * 2U)



static uint16_t adc_buffer[ANALOG_BUFFER_N];

static float s_lpf_voltage;
static float s_lpf_temp;
static volatile float s_voltage;
static volatile float s_temperature;
static volatile uint16_t s_voltage_code;
static uint8_t s_seeded;


/* 温度通道原始码 -> 摄氏度。优先用出厂校准值，不可用时退回数据手册典型值。 */
static float analog_raw_to_celsius(float raw)
{
    const uint16_t cal1 = *TEMP_CAL1_ADDR;
    const uint16_t cal2 = *TEMP_CAL2_ADDR;
    const float raw_norm = raw * ADC_VREF / TEMP_CAL_VREF;

    if ((cal1 == 0U) || (cal1 == 0xFFFFU) ||
        (cal2 == 0U) || (cal2 == 0xFFFFU) || (cal1 == cal2))
    {
        const float vsense = raw_norm * TEMP_CAL_VREF / ADC_FULL_SCALE;

        return (TEMP_TYP_V25 - vsense) / TEMP_TYP_AVG_SLOPE + TEMP_TYP_AMBIENT;
    }

    return (raw_norm - (float)cal1) * (TEMP_CAL2_DEGC - TEMP_CAL1_DEGC)
           / ((float)cal2 - (float)cal1) + TEMP_CAL1_DEGC;
}


/* 处理已填满的半区：偶数下标是电压，奇数下标是温度 */
static void analog_process_block(const uint16_t *block)
{
    uint32_t v_sum = 0U;
    uint32_t t_sum = 0U;
    float v_avg;
    float t_avg;
    uint32_t i;

    for (i = 0U; i < ANALOG_HALF_N; i += ANALOG_CHANNELS)
    {
        v_sum += block[i];
        t_sum += block[i + 1U];
    }

    v_avg = (float)v_sum / (float)ANALOG_AVG_N;
    t_avg = (float)t_sum / (float)ANALOG_AVG_N;

    /*
     * 发布本块电压通道的**平均**码值（偶数下标才是电压）。
     * 不要改成只取单个样点：见 Analog.h 里 Analog_ReadVoltageCode() 的说明。
     */
    s_voltage_code = (uint16_t)(v_avg + 0.5f);

    /* 首轮直接采信，避免从 0 慢慢爬上来 */
    if (s_seeded == 0U)
    {
        s_lpf_voltage = v_avg;
        s_lpf_temp = t_avg;
        s_seeded = 1U;
    }
    else
    {
        s_lpf_voltage += (v_avg - s_lpf_voltage) * ANALOG_LPF_ALPHA;
        s_lpf_temp += (t_avg - s_lpf_temp) * ANALOG_LPF_ALPHA;
    }

    s_voltage = s_lpf_voltage * VOLTAGE_RATIO / ADC_FULL_SCALE;
    s_temperature = analog_raw_to_celsius(s_lpf_temp);
}


void Analog_Init(void)
{
    uint32_t guard;
    uint32_t target;

    s_lpf_voltage = 0.0f;
    s_lpf_temp = 0.0f;
    s_voltage = 0.0f;
    s_temperature = 0.0f;
    s_voltage_code = 0U;
    s_seeded = 0U;

    /*
     * 必须先停 ADC / 清外部触发再武装 DMA：TIM8 既是背光 PWM 又是 ADC 触发源，
     * 屏幕初始化时就已启动；直接 Start_DMA 会让 DMA 从序列中间接手，
     * 缓冲错位一个转换（偶数下标变成温度通道）。
     */
    __HAL_ADC_DISABLE(&hadc1);
    CLEAR_BIT(hadc1.Instance->CR2, ADC_CR2_EXTEN | ADC_CR2_DMA);
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC | ADC_FLAG_OVR);

    /* EXTEN=0 -> HAL 补发软件启动，第一个搬进缓冲的就是 rank1。 */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ANALOG_BUFFER_N);

    /* 等这一轮搬完：用 NDTR（DMA 取走 DR 时已清 EOC）；guard 仅防呆。 */
    target = ANALOG_BUFFER_N - ANALOG_CHANNELS;
    for (guard = 0U; guard < 100000U; ++guard)
    {
        if (hadc1.DMA_Handle->Instance->NDTR <= target)
        {
            break;
        }
    }

    /* 相位已定，交还 TIM8 触发（EXTEN=01 上升沿）。 */
    SET_BIT(hadc1.Instance->CR2, ADC_CR2_EXTEN_0);
}


float Analog_ReadVoltage(void)
{
    return s_voltage;
}


float Analog_ReadTemperature(void)
{
    return s_temperature;
}


uint16_t Analog_ReadVoltageCode(void)
{
    return s_voltage_code;
}


/* HT：前半区 [0..15] 已满，DMA 正在写后半区 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1)
    {
        analog_process_block(&adc_buffer[0]);
    }
}


/* TC：后半区 [16..31] 已满，DMA 正在写前半区 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1)
    {
        analog_process_block(&adc_buffer[ANALOG_HALF_N]);
    }
}
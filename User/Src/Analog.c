#include "Analog.h"


float Analog_Read(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
    float voltage = (float)adc_value * ANALOG_FULL_SCALE_VOLTAGE / 4095.0f;
    return voltage;
}
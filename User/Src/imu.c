#include "imu.h"
#include "spi.h"
#include "LED.h"

void imu_transmit(uint8_t *data, uint16_t size) 
{
    HAL_SPI_Transmit(&hspi3, data, size, 2);
}





void imu_init(void) 
{
    uint8_t tx_buf[2] = {0x8F, 0x00};
    uint8_t rx_buf[2] = {0};
    
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi3, tx_buf, rx_buf, 2, 2);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    
    if (rx_buf[1] == 0x6B) 
    {
        LED2(1);
    }

}
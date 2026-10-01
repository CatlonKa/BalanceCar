#include "TFT_Driver.h"
#include "spi.h"

/** SPI 发送完成：只把本屏所用实例的事件交给驱动。 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == TFT_SPI_HANDLE.Instance) {
        TFT_DriverHandleTxComplete();
    }
}


void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == TFT_SPI_HANDLE.Instance) {
        TFT_DriverHandleError();
    }
}

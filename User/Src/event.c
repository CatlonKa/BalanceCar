#include "event.h"
#include "imu.h"
#include "Attitude.h"
#include "main.h"


int16_t speedL = 0;
int16_t speedR = 0;

uint8_t car_run = 1;
volatile uint8_t car_state_changed = 0;

/* IMU 原始快照：主循环读（非中断上下文），顺序 X/Y/Z，单位 LSB */
volatile int16_t imu_gyro[3];
volatile int16_t imu_acc[3];
volatile uint8_t imu_drdy_flag;

void event_init(void)
{
    HAL_TIM_Base_Start_IT(&htim5);
    HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //编码器
    if (htim == &htim6)
    {
        speedL = encoder_get_delta_left();
        speedR = encoder_get_delta_right();
    }


    else if (htim == &htim5)
    {
        key_tick();
        car_state();
        buzzer_tick();
        Attitude_Update();
    }
}

/* 12B 自 OUTX_L_G(0x22) 起：陀螺 X/Y/Z 6B + 加速度 X/Y/Z 6B，小端 */
static void imu_update(void)
{
    uint8_t buf[12];
    uint16_t i;

    /* 读失败就保留上一组，不发布零值 */
    if (!imu_read_data(buf, 12U))
    {
        return;
    }

    for (i = 0U; i < 3U; i++)
    {
        imu_gyro[i] = (int16_t)((uint16_t)buf[2U * i + 1U] << 8 | buf[2U * i]);
        imu_acc[i] = (int16_t)((uint16_t)buf[6U + 2U * i + 1U] << 8 | buf[6U + 2U * i]);
    }
}

/* 数据就绪中断：只置标志，读 SPI 交给主循环（ISR 内 tick 冻结，超时失效） */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == IMU_INT1_Pin)
    {
        imu_drdy_flag = 1U;
    }
}

/* 主循环调用：有新数据就读一次刷新快照 */
void imu_service(void)
{
    if (imu_drdy_flag == 0U)
    {
        return;
    }
    imu_drdy_flag = 0U;
    imu_update();
}

void car_state(void)
{
    if(!key0_flag)
    {
        return;
    }
    key0_flag = 0;

    if(car_run)
    {
        car_run = 0;
        HAL_TIM_Base_Stop_IT(&htim6);
    }
    else
    {
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        __HAL_TIM_SET_COUNTER(&htim4, 0);
        car_run = 1;
        HAL_TIM_Base_Start_IT(&htim6);
        buzzer_startup_tone();
    }

    car_state_changed = 1;
}
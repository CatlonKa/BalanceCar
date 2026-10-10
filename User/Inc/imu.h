#ifndef __IMU_H__
#define __IMU_H__

#include "main.h"

/* 返回码 */
#define IMU_OK               0
#define IMU_ERR_NO_DEVICE   -1    /* WHO_AM_I 重试超限 */

/* ---------------- LSM6DSR 寄存器地址（数据手册 DocID030186 Rev 1） ---------------- */

#define IMU_REG_FUNC_CFG_ACCESS   0x01
#define IMU_REG_PIN_CTRL          0x02
#define IMU_REG_S4S_TPH_L         0x04
#define IMU_REG_S4S_TPH_H         0x05
#define IMU_REG_S4S_RR            0x06
#define IMU_REG_FIFO_CTRL1        0x07
#define IMU_REG_FIFO_CTRL2        0x08
#define IMU_REG_FIFO_CTRL3        0x09
#define IMU_REG_FIFO_CTRL4        0x0A
#define IMU_REG_COUNTER_BDR_REG1  0x0B
#define IMU_REG_COUNTER_BDR_REG2  0x0C
#define IMU_REG_INT1_CTRL         0x0D
#define IMU_REG_INT2_CTRL         0x0E
#define IMU_REG_WHO_AM_I          0x0F
#define IMU_REG_CTRL1_XL          0x10
#define IMU_REG_CTRL2_G           0x11
#define IMU_REG_CTRL3_C           0x12
#define IMU_REG_CTRL4_C           0x13
#define IMU_REG_CTRL5_C           0x14
#define IMU_REG_CTRL6_C           0x15
#define IMU_REG_CTRL7_G           0x16
#define IMU_REG_CTRL8_XL          0x17
#define IMU_REG_CTRL9_XL          0x18
#define IMU_REG_CTRL10_C          0x19
#define IMU_REG_TAP_CFG0          0x56
#define IMU_REG_TAP_CFG1          0x57
#define IMU_REG_TAP_CFG2          0x58
#define IMU_REG_TAP_THS_6D        0x59
#define IMU_REG_INT_DUR2          0x5A
#define IMU_REG_WAKE_UP_THS       0x5B
#define IMU_REG_WAKE_UP_DUR       0x5C
#define IMU_REG_FREE_FALL         0x5D
#define IMU_REG_MD1_CFG           0x5E
#define IMU_REG_MD2_CFG           0x5F
#define IMU_REG_S4S_ST_CMD_CODE   0x60
#define IMU_REG_S4S_DT_REG        0x61
#define IMU_REG_I3C_BUS_AVB       0x62
#define IMU_REG_X_OFS_USR         0x73
#define IMU_REG_Y_OFS_USR         0x74
#define IMU_REG_Z_OFS_USR         0x75
/* 陀螺 6B + 加速度 6B 连续，可一次读完 */
#define IMU_REG_OUTX_L_G          0x22

/* WHO_AM_I 只读，固定值 */
#define IMU_WHO_AM_I_VAL          0x6B

/* ============ 上电写入的配置值：要改就改这里 ============ */

/* 0x01 FUNC_CFG_ACCESS：bit7=功能页 bit6=传感器枢纽页；设备工作必须为 0 */
#define IMU_CFG_FUNC_CFG_ACCESS   0x00

/* 0x02 PIN_CTRL：bit7=OIS_PU_DIS bit6=SDO_PU_EN；bit5:0 保留(必须为 1)，复位值 0x3F */
#define IMU_CFG_PIN_CTRL          0x3F

/* 0x04/0x05/0x06 S4S 时间帧与分辨率；全 0 = 不用 S4S 传感器同步 */
#define IMU_CFG_S4S_TPH_L         0x00
#define IMU_CFG_S4S_TPH_H         0x00
#define IMU_CFG_S4S_RR            0x00

/* 0x07/0x08 FIFO 水位 WTM[8:0]，1 LSB = 1 组（6B 数据 + 1B TAG） */
#define IMU_CFG_FIFO_CTRL1        0x00
#define IMU_CFG_FIFO_CTRL2        0x00

/* 0x09 FIFO_CTRL3：bit7:4=BDR_GY bit3:0=BDR_XL，0 = 不入 FIFO */
#define IMU_CFG_FIFO_CTRL3        0x00

/* 0x0A FIFO_CTRL4：bit2:0=FIFO_MODE，000=Bypass（FIFO 关闭） */
#define IMU_CFG_FIFO_CTRL4        0x00

/* 0x0B/0x0C 批处理事件计数阈值 CNT_BDR_TH[10:0] */
#define IMU_CFG_COUNTER_BDR_REG1  0x00
#define IMU_CFG_COUNTER_BDR_REG2  0x00

/* 0x0D INT1_CTRL：bit7:0 = DEN_DRDY/CNT_BDR/FIFO_FULL/FIFO_OVR/FIFO_TH/BOOT/DRDY_G/DRDY_XL */
#define IMU_CFG_INT1_CTRL         0x00

/* 0x0E INT2_CTRL：bit6:0 = CNT_BDR/FIFO_FULL/FIFO_OVR/FIFO_TH/DRDY_TEMP/DRDY_G/DRDY_XL */
#define IMU_CFG_INT2_CTRL         0x00

/* 0x10 CTRL1_XL：bit7:4=ODR_XL bit3:2=FS_XL bit1=LPF2_XL_EN
   ODR 0000关/0010=26/0011=52/0100=104/0101=208/0110=416/0111=833/1000=1.66k/1001=3.33k/1010=6.66k
   FS  00=±2g/01=±16g/10=±4g/11=±8g；LPF2_XL_EN=1 取二级滤波输出 */
#define IMU_CFG_CTRL1_XL          0x88   /* 1.66 kHz，±4 g */

/* 0x11 CTRL2_G：bit7:4=ODR_G bit3:2=FS_G bit1=FS_125 bit0=FS_4000
   FS_G 00=250/01=500/10=1000/11=2000 dps；FS_125 或 FS_4000 置 1 时覆盖 FS_G */
#define IMU_CFG_CTRL2_G           0x84   /* 1.66 kHz，±500 dps */

/* 0x12 CTRL3_C：bit7=BOOT bit6=BDU bit5=H_LACTIVE bit4=PP_OD bit3=SIM
   bit2=IF_INC bit1=保留 bit0=SW_RESET（自动清 0） */
#define IMU_CFG_CTRL3_C           0x44   /* BDU=1，IF_INC=1 */

/* 0x13 CTRL4_C：bit6=SLEEP_G bit5=INT2_on_INT1 bit4=DRDY_MASK
   bit3=I2C_disable bit2=LPF1_SEL_G，其余保留 */
#define IMU_CFG_CTRL4_C           0x00

/* 0x14 CTRL5_C：bit5:4=ROUNDING bit3:2=ST_G bit1:0=ST_XL（自检，0=关闭） */
#define IMU_CFG_CTRL5_C           0x00

/* 0x15 CTRL6_C：bit7=TRIG_EN bit6=LVL1_EN bit5=LVL2_EN bit4=XL_HM_MODE
   bit3=USR_OFF_W bit2:0=FTYPE（陀螺 LPF1 带宽，手册表 66） */
#define IMU_CFG_CTRL6_C           0x00

/* 0x16 CTRL7_G：bit7=G_HM_MODE bit6=HP_EN_G bit5:4=HPM_G bit3=保留(必须0)
   bit2=OIS_ON_EN bit1=USR_OFF_ON_OUT bit0=OIS_ON */
#define IMU_CFG_CTRL7_G           0x00

/* 0x17 CTRL8_XL：bit7:5=HPCF_XL bit4=HP_REF_MODE_XL bit3=FASTSETTL_MODE_XL
   bit2=HP_SLOPE_XL_EN bit1=保留(必须0) bit0=LOW_PASS_ON_6D */
#define IMU_CFG_CTRL8_XL          0x00

/* 0x18 CTRL9_XL：bit7:5=DEN_X/Y/Z bit4=DEN_XL_G bit3=DEN_XL_EN bit2=DEN_LH
   bit1=I3C_disable bit0=保留(必须0)；复位值就是 0xE0 */
#define IMU_CFG_CTRL9_XL          0xE0

/* 0x19 CTRL10_C：bit5=TIMESTAMP_EN，其余保留 */
#define IMU_CFG_CTRL10_C          0x00

/* 0x56 TAP_CFG0：bit6=INT_CLR_ON_READ bit5=SLEEP_STATUS_ON_INT bit4=SLOPE_FDS
   bit3:1=TAP_X/Y/Z_EN bit0=LIR */
#define IMU_CFG_TAP_CFG0          0x00

/* 0x57 TAP_CFG1：bit7:5=TAP_PRIORITY bit4:0=TAP_THS_X（1 LSB = FS_XL/32） */
#define IMU_CFG_TAP_CFG1          0x00

/* 0x58 TAP_CFG2：bit7=INTERRUPTS_ENABLE bit6:5=INACT_EN bit4:0=TAP_THS_Y */
#define IMU_CFG_TAP_CFG2          0x00

/* 0x59 TAP_THS_6D：bit7=D4D_EN bit6:5=SIXD_THS bit4:0=TAP_THS_Z */
#define IMU_CFG_TAP_THS_6D        0x00

/* 0x5A INT_DUR2：bit7:4=DUR bit3:2=QUIET bit1:0=SHOCK（双击时序） */
#define IMU_CFG_INT_DUR2          0x00

/* 0x5B WAKE_UP_THS：bit7=SINGLE_DOUBLE_TAP bit6=USR_OFF_ON_WU bit5:0=WK_THS */
#define IMU_CFG_WAKE_UP_THS       0x00

/* 0x5C WAKE_UP_DUR：bit7=FF_DUR5 bit6:5=WAKE_DUR bit4=WAKE_THS_W bit3:0=SLEEP_DUR */
#define IMU_CFG_WAKE_UP_DUR       0x00

/* 0x5D FREE_FALL：bit7:3=FF_DUR bit2:0=FF_THS（156/219/250/312/344/406/469/500 mg） */
#define IMU_CFG_FREE_FALL         0x00

/* 0x5E MD1_CFG：bit7..0 = SLEEP_CHANGE/SINGLE_TAP/WU/FF/DOUBLE_TAP/6D/EMB_FUNC/SHUB */
#define IMU_CFG_MD1_CFG           0x00

/* 0x5F MD2_CFG：bit7..0 = SLEEP_CHANGE/SINGLE_TAP/WU/FF/DOUBLE_TAP/6D/EMB_FUNC/TIMESTAMP */
#define IMU_CFG_MD2_CFG           0x00

/* 0x60/0x61/0x62 S4S 主命令码 / DT / I3C 总线可用时间 */
#define IMU_CFG_S4S_ST_CMD_CODE   0x00
#define IMU_CFG_S4S_DT_REG        0x00
#define IMU_CFG_I3C_BUS_AVB       0x00

/* 0x73/0x74/0x75 加速度计用户偏置；1 LSB 由 CTRL6_C.USR_OFF_W 选 2^-10 或 2^-6 g */
#define IMU_CFG_X_OFS_USR         0x00
#define IMU_CFG_Y_OFS_USR         0x00
#define IMU_CFG_Z_OFS_USR         0x00

int  imu_init(void);
void imu_transmit(uint8_t *data, uint16_t size);
void imu_read_data(uint8_t *data, uint16_t size);

#endif
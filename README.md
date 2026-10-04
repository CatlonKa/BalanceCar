# Balance

STM32F407 平衡车固件。1bpp TFT 界面（支持彩色图片直推）+ 电机/编码器 + 电池采样，
正在向「可调试平台」演进。

## 硬件

| 部分 | 型号 / 资源 |
|---|---|
| MCU | STM32F407VET6（168 MHz，HSE 8 MHz） |
| 显示 | ST7735S 128×160，SPI1 |
| 姿态 | IMU，SPI3 |
| 电机 | 2 路 H 桥 PH/EN，TIM2 CH2/CH4 |
| 编码器 | 左 TIM1、右 TIM4（TI1 计数） |
| 电池 | ADC1_IN12，39k/10k 分压；另采芯片内部温度传感器 |
| 灯带 | WS2812，TIM3_CH3 + DMA |
| 蜂鸣器 | TIM9_CH1 PWM |
| 摄像头 | OV7725 → DCMI（已初始化，未启用） |
| 无线 | ESP32，SPI2 + USART6（未启用） |
| 调试口 | USART3，115200 8N1（`printf` 已重定向） |

引脚详见 `Balance.ioc`（CubeMX）。

| 功能 | 引脚 |
|---|---|
| TFT | SCK PA5、MOSI PA7、CS PB0、DC PC5、RST PC4、背光 PB1 (TIM8_CH3N) |
| 编码器 | 左 PE9/PE11、右 PD12/PD13 |
| 电机 | PHL PA0、ENL PA1、PHR PA2、ENR PA3 |
| IMU | SCK PB3、MISO PB4、MOSI PB5、CS PD7、INT1 PD5、INT2 PD6 |
| 电池 | PC2 |
| 灯带 | DI PC8 (TIM3_CH3) |
| 蜂鸣器 | PE5 (TIM9_CH1) |
| 指示灯 | LED_W1 PE2、LED_W2 PD0、LED1 PD3、LED2 PD2、LED3 PD1 |
| 调试串口 | TX PB10、RX PB11 |
| 按键 | KEY0 PE7（主动停机）、KEY1 PE15 |
| 摄像头 | DCMI：HSYNC PA4、PIXCLK PA6、VSYNC PB7、D0~D9 (PA9/PA10/PE0/PE1/PC11/PB6/PB8/PB9/PC10/PC12)；SCCB SIOD PD14、SIOC PD15；XCLK PC9、PWND PA11、RST PA12 |
| ESP32 | SPI2 SCK PB13、MISO PB14、MOSI PB15、CS PD9、ESP_CS PD8、IRQ PD10、CE PD11；串口 TX PC6、RX PC7 |

## 构建

需要 `arm-none-eabi-gcc`、CMake ≥ 3.22、Ninja。

```bash
cmake --preset Debug
cmake --build build/Debug
```

产物 `build/Debug/Balance.elf`。也可直接用 VS Code 的 CMake Tools / STM32 扩展。

当前占用：**Flash 198,240 B (37.8%) / RAM 11,304 B (8.6%)**。


## 目录结构

```
Core/                  CubeMX 生成（只改 USER CODE 段）
Drivers/               ST 官方 HAL 与 CMSIS
User/Inc, User/Src/    应用代码
  TFT/                 ST7735S 驱动、图形层与屏幕生命周期
  UI/                  KK_UI 界面运行时与应用层
cmake/                 工具链与 CubeMX 的 CMake 片段
```


## 运行模型

上电默认**电机自转、屏幕不初始化**（动电机时屏幕会被拔掉/断电）。
按 KEY0 停机 → 车轮交回 UI，此时才初始化屏幕并显示界面；再按 KEY0 起步 →
屏幕关显示进睡眠、背光熄灭，并响三声上行自检音（800/1200/1600 Hz）。

## 功能现状

已可用：

- TFT 1bpp 双缓冲 + 差量刷新，支持保留帧与脏矩形局部重绘
- KK_UI 五页界面（首页 / 菜单 / 状态 / 波形 / 图片），首页竖向轮播，编码器手势 + 全局后退
- 彩色开机图（RGB565 绕过帧缓冲直推，三张轮换）
- 电机 PWM（14 kHz）、编码器测速、WS2812、IMU 初始化
- 电池电压 + 芯片温度：TIM8_TRGO 触发 ADC 双通道 → DMA 乒乓 → IIR 低通；
  状态页显示真实电压，波形页画真实采样（固定量程 11.1~12.6 V）
- 蜂鸣器提示音，时序由 TIM5 中断推进
- 调试串口输出

进行中 / 未完成：

- 状态页的**角度**、**速度**两行仍是占位
- 电机闭环（PID）未实现，`motor.c` 只有方向与占空比接口
- DCMI + OV7725 已初始化，尚未开始采集（缺 SCCB 配置与帧缓冲规划）
- ESP32 链路（SPI2）未启用，无 DMA/中断
- `main.c` 主循环仍有 `HAL_Delay(5)` 与 LED 测试代码

## 许可证

项目本体采用 **[PolyForm Noncommercial License 1.0.0](LICENSE.txt)**。

**可以**：任意非商业用途，包括个人学习、研究、实验、业余爱好项目，
以及学校、科研机构、公益组织等的使用。可以自由修改、分发。

**不可以**：任何带商业预期的用途（用于产品、对外提供服务、公司内部项目等）。

分发时必须附带本许可证与上面的 `Required Notice` 行。

`Drivers/` 下的 ST 官方 HAL 与 CMSIS 保留各自的原许可证（BSD-3-Clause / Apache-2.0），
不受本项目许可证影响。`User/Inc/TFT/TFT_LICENSE.txt`、`User/Inc/UI/KK_UI_LICENSE.txt`
对应各自的运行库来源（均为 MIT）。


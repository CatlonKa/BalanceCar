# Balance

STM32F407 平衡车固件。单色 TFT 界面 + 电机/编码器闭环 + 电池采样，正在向「可调试平台」演进。

## 硬件

| 部分 | 型号 / 资源 |
|---|---|
| MCU | STM32F407VET6（168 MHz，HSE 8 MHz） |
| 显示 | ST7735S 128×160，SPI1 |
| 姿态 | IMU，SPI3 |
| 电机 | 2 路 H 桥 PH/EN，TIM2 CH2/CH4 |
| 编码器 | 左 TIM1、右 TIM4（TI1 计数） |
| 电池 | ADC1_IN12，39k/10k 分压 |
| 灯带 | WS2812，TIM3_CH3 + DMA |
| 调试口 | USART3，115200 8N1（`printf` 已重定向） |

引脚详见 `Balance.ioc`（CubeMX）。

| 功能 | 引脚 |
|---|---|
| TFT | SCK PA5、MOSI PA7、CS PB0、DC PC5、RST PC4、背光 PB1 (TIM8_CH3N) |
| 编码器 | 左 PE9/PE11、右 PD12/PD13 |
| 电机 | PHL PA0、ENL PA1、PHR PA2、ENR PA3 |
| IMU | SCK PB3、MISO PB4、MOSI PB5、CS PD7 |
| 电池 | PC2 |
| 调试串口 | TX PB10、RX PB11 |
| 按键 | KEY0 PE7（主动停机）、KEY1 PE15 |

## 构建

需要 `arm-none-eabi-gcc`、CMake ≥ 3.22、Ninja。

```bash
cmake --preset Debug
cmake --build build/Debug
```

产物 `build/Debug/Balance.elf`。也可直接用 VS Code 的 CMake Tools / STM32 扩展。

当前占用：**Flash 74,916 B / RAM 11,520 B**。

> 注意：工程链接的是 newlib-nano，**它的 `printf` 不支持 `%f`**
> 因此浮点一律换算成整数再打印，例如电压用毫伏。

## 目录结构

```
Core/                  CubeMX 生成（只改 USER CODE 段）
Drivers/               ST 官方 HAL 与 CMSIS
User/Inc, User/Src/    应用代码
  TFT/                 ST7735S 驱动与图形层
  UI/                  KK_UI 界面运行时与应用层
cmake/                 工具链与 CubeMX 的 CMake 片段
```

约定：

- 屏幕相关代码只放在 `User/{Inc,Src}/TFT/`，符号统一 `TFT_` 前缀。
- 界面逻辑放在 `User/{Inc,Src}/UI/`，界面绘制与刷新所有权归 KK_UI。
- 新增 `.c` 必须手动登记到根 `CMakeLists.txt` 的 `target_sources`。

## 功能现状

已可用：

- TFT 1bpp 双缓冲 + 差量刷新，支持保留帧与脏矩形局部重绘
- KK_UI 四页界面（首页 / 菜单 / 状态 / 波形），编码器手势，全局后退
- 电机 PWM、编码器测速、WS2812、IMU 初始化
- 电池电压采样与调试串口输出

进行中 / 未完成：

- 状态页与波形页的数据源仍是占位，未接真实车辆数据
- 波形页尚未接入 ADC 采样
- DCMI + OV7725 已初始化，尚未开始采集
- ESP32 链路（SPI2）未启用

待办与需求见 `MOTOR_UI_TODO.md`。

## 许可证

项目本体采用 **[PolyForm Noncommercial License 1.0.0](LICENSE.txt)**。

**可以**：任意非商业用途，包括个人学习、研究、实验、业余爱好项目，
以及学校、科研机构、公益组织等的使用。可以自由修改、分发。

**不可以**：任何带商业预期的用途（用于产品、对外提供服务、公司内部项目等）。

分发时必须附带本许可证与上面的 `Required Notice` 行。

> 严格来说这属于「源码可见」（source-available），而不是 OSI 定义的「开源」——
> OSI 要求许可证不得限制使用领域。这不影响你按上面的范围使用。

`Drivers/` 下的 ST 官方 HAL 与 CMSIS 保留各自的原许可证（BSD-3-Clause / Apache-2.0），
不受本项目许可证影响。`User/Inc/TFT/TFT_LICENSE.txt`、`User/Inc/UI/KK_UI_LICENSE.txt`
对应各自的运行库来源（均为 MIT）。


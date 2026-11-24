# HANDOFF — Balance 直立环调参与无线遥测链路

- 生成时间：2026-11-14
- 工作区：`D:\1self_project\Balance`（STM32F407 两轮自平衡车，CMake + Ninja + HAL）
- 当前分支：`main`，HEAD `c130591`「零度稳定维持平衡状态」
- 本次会话主题：直立环调参失败复盘 → 遥测层重构 → nRF24 无线接入

---

## 1. 用户当前的核心诉求（按优先级）

1. **车要能在重心可变范围内自平衡**（电池位置每次不同，不能每次手填平衡角）
   - 原话：「能够直立不动，不需要回到放置时的原点，但不能逐渐向前或向后跑」
   - 即：稳态速度 = 0，允许位置慢漂；**不要求回原点**
2. **先做线加速度补偿**（用户明确指定优先）：持续加速时 `pitch` 被拉向 0
3. **需要无线调参**：没有 J-Link，RTT 不可用；现在是有线 USART3
   - 已确认手上有**一对 nRF24L01**，主要用途**只上行看波形**（暂不做下行改参数）
4. **流程约束（重要）**：一步一步做，**每步必须用户自己在硬件上验证可用后**才进下一步

---

## 2. 已完成的改动（未提交，工作区脏）

`git status --short` 当前状态：
```
 M CMakeLists.txt
 M Core/Src/main.c
 M User/Inc/Attitude.h
 M User/Inc/Control/Control.h
 M User/Inc/Control/Control_PID.h
 M User/Inc/Rtt.h
 M User/Inc/event.h
 M User/Src/Attitude.c
 M User/Src/Control/Control.c
 M User/Src/Control/Control_PID.c
 M User/Src/Rtt.c
 M User/Src/event.c
?? User/Inc/Nrf24.h
?? User/Inc/Telemetry.h
?? User/Src/Nrf24.c
?? User/Src/Telemetry.c
?? docs/11.14.md
```

### 2.1 Phase A：遥测层（已完成，构建通过）

- 新建 `User/Inc/Telemetry.h` + `User/Src/Telemetry.c`：采集 → 打包 → 传输三层分离
- sink 三选一：`TELEMETRY_SINK_UART`（默认）/ `_RTT` / `_NRF`
- UART 帧：`0xAA55 | seq(u16) | 9×float | crc16(CCITT-FALSE)` = **42 B，100 Hz**
  （不是 200 Hz：42 B × 10 bit ÷ 115200 = 3.65 ms/帧，200 Hz 会占主循环 73%）
- 9 路字段顺序：`pitch, pitch_rate, speed_avg, turn_rate, vbat, pitch_set, pwm_common, acc_tilt, wheel_dist`
- 新增导出接口：
  - `Attitude_GetAccTilt()`（`Attitude.c` 新增 `static s_acc_tilt`）
  - `Control_PID_GetPitchSet()`（新增 `static s_pitch_set`）
  - `Control_GetPwmCommon()`（新增 `static s_pwm_common` = (p_L+p_R)/2，三个早退分支全部清零）
  - `event.c` 新增 `volatile int32_t wheel_dist`（TIM6 里 `+= speedL`，**仅 car_run=1 时累加**）
- `Rtt.c`：删除 `Rtt_Telemetry()`（职责并入 Telemetry）；`RTT_TELEMETRY_CH_COUNT` 5→9；
  `RTT_CH1_BUFFER_SIZE` 4000→**4032**（必须整除帧长 36 B）
- `main.c`：`Rtt_Init()` → `Telemetry_Init()`，`Rtt_Telemetry()` → `Telemetry_Update()`
- 工具 `tools/telemetry_live.py`：找帧 + CRC + 丢帧统计 + `--csv` / `--plot` / `--mark`
- **构建：Flash 218,076 B / RAM 12,000 B（RAM 降 4,304 B，因为 UART sink 下 Rtt.c 的
  4032 B 缓冲被 `--gc-sections` 丢弃）**

### 2.2 Phase C：nRF24 驱动（代码完成，但**硬件参数未就绪**）

- 新建 `User/Inc/Nrf24.h` + `User/Src/Nrf24.c`：寄存器级驱动，直接读写 `SPI2->DR`（不用 HAL）
  - `bool nrf_init(void)`：回读 `RF_CH` 自检 SPI 是否通
  - `bool nrf_send(const uint8_t*, uint8_t)`：FIFO 满**直接丢帧不等待**
- NRF sink 包：`seq(u16) + 7×float + crc16` = **32 B 正好一包**
  - `TELEMETRY_NRF_FIELDS = 7`（前 7 路；`acc_tilt` / `wheel_dist` 只走 UART）
- 工具 `tools/nrf24_rx.py`：定长 32 B 切包 + CRC 滑动对齐 + 包率/丢包统计 + `--csv`
- 配置：`CONFIG=0x0E`、`EN_AA=0`、`RF_CH=76`、`RF_SETUP=0x06`（1 Mbps / 0 dBm）、
  `SETUP_AW=5B`、`RX_PW_P0=32`、地址 `"BALN0"`
- **构建：Flash 218,116 B / RAM 12,000 B**

### 2.3 文档

- `docs/11.14.md`：完整计划（体检结论、NRF 可行性核对、Phase A~E 步骤、已知坑）

---

## 3. ❗阻塞项（下一步必须先解决的）

### 3.1 SPI2 预分频必须由用户在 CubeMX 改

- `Core/Src/spi.c` 现在是 `SPI_BAUDRATEPRESCALER_2` = **21 MHz**
- nRF24 的 SPI 上限是 **10 MHz** → 现在会大量掉包或完全不响应
- **用户在 CubeMX 里改：`Connectivity → SPI2 → Parameter Settings → Clock Parameters →
  Prescaler (for Baud Rate): 2 → 8`（→ 5.25 MBits/s），然后 GENERATE CODE**
- `.ioc` 等价写法：
  ```
  SPI2.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_8
  SPI2.CalculateBaudRate=5.25 MBits/s
  ```
- ⚠️ **不要自己改生成代码**（用户已撤销我在 `USER CODE BEGIN SPI2_Init 2` 里的覆盖）

### 3.2 用户明确的新约定（已写入 repo memory）

> 以后 CubeMX 生成的文件的时钟/外设参数，**一律由用户在 CubeMX 里改**，
> 不要直接编辑，也不要在 USER CODE 段写覆盖补丁。
> 正确做法 = 说清在哪个界面改哪个值，用户重新生成。

### 3.3 待测的两个数（有了才能算准后续所有增益）

| 量 | 怎么测 | 影响 |
|---|---|---|
| `k_x`（counts/m） | 车扶稳→记 `wheel_dist`→分 5 段各推 20 cm→再记差值 | 速度环全部增益 |
| `d0`（电机死区） | 需先加临时定值输出（Phase B2 待做） | 死区前馈量、KD 上限 |

⚠️ `k_x` 测量时 **`car_run` 必须为 1**（别按 KEY0），否则 `wheel_dist` 不累加。
分段推 20 cm × 5 = 1.00 m，USB 线不拉扯（用户反馈"得接远程才能测"的规避方案）。

---

## 4. 关键技术结论（本次会话推导出的，勿重复推演）

### 4.1 上一轮「积分自学重心」为什么失败（已全部回退）

- 根因是**量纲**：`s_speed_int += speed * KI * dt` 中 `speed` 是「计数/2ms」，
  控制频率 1 kHz（零阶保持，每值用两次）→ 实测走 **1 m 只积 0.0065 rad**（差约 30 倍）
- 同时 `KP * speed`（KP=0.001、speed=100）= 0.1 rad **过大**
- 结果 `pitch_set ≈ 0`，等于仍然要求站水平 → 必然前跑
- **教训：在当前单位体系下，速度积分没有可用的物理含义；不要再走「积分自动学中值」。**
- 另一次失败：把死区前馈放进 `control_limit()` —— 那是**逐轮**调用，
  会同时放大 `steer` 3 倍，并把 `|thrust| < d0` 变成继电器式 ±d0 跳变，直接毁平衡。
  **死区只能加共模。**

### 4.2 参数与公式（按 k/h ≈ 0.05、g/h ≈ 98 估算）

内环（直立 PD）：
```
wn   = sqrt((k/h) * KP - g/h)
zeta = (k/h) * KD / (2 * wn)
```
- KP = 3000 → wn = 7.2 rad/s，周期 T = 0.87 s
- KD = 120 → zeta = 0.42（欠阻尼 = 「小幅来回走」）
- zeta = 0.7 需要 KD ≈ 202

外环（速度 PI）：
```
KI_phys = KI * k_x / 500        （speed 是 2 ms 窗口计数）
wn_o    = sqrt(g * KI_phys)
漂移距离 d = 偏差 / KI_phys      （与 SPEED_KP 无关）
```
- 当前 KI = 0.0005 → KI_phys = 0.0065 → wn_o = 0.25 rad/s（τ ≈ 16 s），慢约 4 倍
- 偏差 0.01 rad 时漂移 1.54 m

转向：
- `turn_rate` 分辨率 0.5 计数；按 k_x = 6500、L = 0.15 m，1 个计数差 ≈ 1 rad/s
- 慢自转时 `steer` 只有 4~8 PWM，**低于死区 → 纯 P 无效，必须带积分**

编码器线加速度：
- 直接差分 = **38.5 m/s² 每计数**，完全不可用
- 先低通速度（τ = 20 ms）再差分，噪声 ≈ 0.25 m/s²，SNR ≈ 8，可用

### 4.3 持续加速时 `pitch` 被拉向 0（用户指定优先做）

- 加速度计测比力：`acc[Y] = g·sinθ − a·cosθ`，`acc[Z] = g·cosθ + a·sinθ`
- 平衡态 `a = g·tanθ` 代入 → `acc[Y] = 0`，`acc[Z] = g/cosθ`
- 即 `acc_tilt = atan2(Y,Z) = 0` → **报告「完全直立」**，互补滤波（τ≈50 ms）把 `pitch` 拉向 0
- **精确反解（不能只减 Y 轴）**：`y = acc[1] + a·cosθ`，`z = acc[2] − a·sinθ`（θ 用上一拍 pitch）

### 4.4 其他已知问题

- `Attitude` 现式 `pitch += α·rate·dt + (1−α)·diff` 把陀螺积分缩了 2%；
  做权重门控时应改成 `pitch += rate·dt + (1−α)·w·diff`
- `speedL/speedR` 是 `int16_t` 但**无 `volatile`**；`0.5f*(speedL+speedR)` 是两次独立读，非原子
- `HAL_SPI_Init` **不置 SPE** → 寄存器级访问前必须 `__HAL_SPI_ENABLE()`（nRF24 与 IMU 同坑）
- nRF24 的 IRQ 脚 PD10 是**普通输入（非 EXTI）**，代码不依赖它，靠读 `STATUS`
- **nRF24 模块 VCC-GND 必须就近焊 10 µF + 100 nF**，否则有刷电机 14 kHz PWM +
  换向尖峰会大量掉包

---

## 5. 下一步（建议顺序）

### Step 1（用户硬件验证 Phase A）
1. 烧录（当前 sink = UART）
2. `pip install pyserial` → `python tools/telemetry_live.py -p COM10`
3. 四条判据：
   - `crc_err` 恒 0
   - `lost` 恒 0
   - 静止时 `pitch_set ≈ TRIM`（−0.06）
   - 推车时 `wheel_dist` 单调变化（`car_run=1`）

### Step 2（量 k_x）
分 5 段各推 20 cm，`k_x = D5 − D0`（走满 1.00 m 时数值即 counts/m）

### Step 3（用户在 CubeMX 改 SPI2 分频 2→8，见 §3.1）

### Step 4（切无线）
`Telemetry.h` 把 `TELEMETRY_SINK` 改成 `TELEMETRY_SINK_NRF`，重新构建烧录。
**还需要 PC 侧接收端**：第二块 nRF24 + 任一 3.3V MCU（Arduino/STM32 最小板）
做 USB 桥，收到的 32 B 原样从串口吐出 → `tools/nrf24_bridge`（**尚未编写**，约 20 行）。
⚠️ 用户尚未回复"有没有闲置的 3.3V 开发板"，这一步会卡在这里。

### Step 5（Phase B2：测 d0）
需要临时加一个「定值 PWM 输出」的调试入口（用户手扶车身扫 20/40/60/80/100），
记「起转」「停转」两点得 `d0` 与滞回。⚠️ 这需要新增代码，**先跟用户确认**。

### Step 6（Phase E：回到控制）
- E1 线加速度补偿（用户优先）：500 Hz 入口（`event.c` TIM6 调用），
  先低通速度 τ=20 ms → 差分 → 二次低通 → 钳制 ±10 m/s² → 精确反解
  ⚠️ `a_lin` 这一路遥测字段**当初被我推迟到这里**，加上时要同步改 `Telemetry` 的字段数与上位机
- E2 死区前馈：`Control_Update` 里**拆共模/差模，只补共模**
- E3 转向环：P + I + `turn_rate` 20 ms 低通
- E4 速度环：KI → 0.005~0.008、KP → 0.009~0.011、ILIM → 0.15、
  SET_LIMIT 0.4 → 0.20、TRIM −0.06 → 0（让积分学）
  **以上数值需用实测 k_x 重新计算**
- E5 回访：E2 完成后重试更大的 KD（当前用户已自行把 KD 从 120 调到 **200**）

---

## 6. 当前参数快照（`User/Inc/Control/Control_PID.h`）

```c
#define PID_PITCH_KP        3000.0f
#define PID_PITCH_KD         200.0f    /* 用户已从 120 改到 200 */
#define PID_SPEED_KP        0.001f
#define PID_SPEED_KI        0.0005f
#define PID_SPEED_ILIM      0.05f
#define PID_PITCH_SET_LIMIT 0.4f
#define PID_PITCH_TRIM_RAD  -0.06f
#define PID_TURN_KP         0.0f
```

用户实测反馈：
- KD > 130 曾出现高频振动（「每次临近平衡点时轮子带动车快速振动」→ 判为死区极限环）
- 已能较稳定直立；直走基本不跑飞（TRIM 当前贴合重心）
- 无法规转向环 → 会自己慢慢转
- 电机死区**未实测**（旧记录 d0 ≈ 29，不可信）

---

## 7. 建议调用的 skills

| skill | 何时用 | 路径 |
|---|---|---|
| **diagnosing-bugs** | 若遥测/无线收不到数据、`crc_err` 不为 0、SPI 无响应 | `c:\Users\guqian\.agents\skills\diagnosing-bugs\SKILL.md` |
| **embedded-serial-debug** | USART3 有线遥测收不到 / 乱码 / 波特率问题 | `c:\Users\guqian\.agents\skills\embedded-serial-debug\SKILL.md` |
| **embedded-register-debug** | nRF24 或 IMU 的 SPI 寄存器读回不对、SPE/时钟未使能 | `c:\Users\guqian\.agents\skills\embedded-register-debug\SKILL.md` |
| **embedded-flash-debug** | ST-Link 烧录失败、`stlinkserver.exe` 抢占探针 | `c:\Users\guqian\.agents\skills\embedded-flash-debug\SKILL.md` |
| **embedded-hil-tuning** | 进入 E4/E5 真正扫增益时（参数可从宏改成可写全局变量后用 `mem_poke`） | `c:\Users\guqian\.agents\skills\embedded-hil-tuning\SKILL.md` |
| **embedded-size-optimize** | 若 Flash/RAM 吃紧（当前 218,116 B / 12,000 B，余量充足，暂不需要） | `c:\Users\guqian\.agents\skills\embedded-size-optimize\SKILL.md` |

---

## 8. 重要工作约定（repo memory 已记录）

1. **注释越短越好**（用户强调多次）：只写「频率 + 用途」「算法名 + 公式」「单位/量纲」
2. **改动保留原有风格**：不改既有标识符、不顺手重构、只改与需求直接相关的行
3. **CubeMX 生成文件的外设参数由用户改**（见 §3.2）
4. 构建：`cube-cmake --build build/Debug`（不要手敲 cmake）
5. 烧录：VS Code 任务「ST-Link：仅烧录」
6. 新增用户源文件**必须手动**加进根 `CMakeLists.txt` 的 `target_sources`

---

## 9. 相关文档（不要重复内容，直接引用）

- `docs/11.14.md` — 本次计划全文（体检结论 / NRF 可行性 / Phase A~E / 已知坑）
- `docs/姿态估计-线性加速度补偿.md` — 线加速度补偿原理与精确反解（§1.2、§2.2、§7）
- `docs/CMSIS-DSP-Guide.md` — 若以后要换 `arm_atan2_f32` 省掉 newlib errno 开销
- `.github/copilot-instructions.md` — 注释与改动范围约定
- `CODING_STANDARDS.md` — 审查标准

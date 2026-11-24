# HANDOFF — Balance 平衡车：交付 8 字 demo 作品

- 生成：2026-11-15
- 工作区：`D:\1self_project\Balance`（STM32F407 两轮自平衡车，CMake + Ninja + HAL）
- 分支 `main`，HEAD `c130591`「零度稳定维持平衡状态」—— **工作区脏，全部改动未提交**
- 下一阶段焦点：**交出「直走 → 右弧 → 直走 → 左弧」的 8 字轨迹 demo**

> 本文件只写「文档里没有的东西」。架构、路线、剧本、死区分析都在下列文档中，**不要重复推导**：
>
> | 文档 | 内容 |
> |---|---|
> | `docs/11.14.md` | **架构与路线**：阶段划分、长期接口 F/G/H、参数现状、已知坑、暂缓项 |
> | `User/Src/Demo/README.md` | **demo 实操**：剧本表、调参顺序、验收标准、完整移除方法 |
> | `docs/死区问题-技术归纳.md` | 死区/抖动 459 行完整分析（已交外部专家，结论：开环常数前馈不适用） |
> | `docs/HANDOFF-balance-2026-11-14.md` | 上一轮交接（遥测层与 nRF24 引入） |
> | `.github/copilot-instructions.md` | 注释要短、改动保留原风格、CubeMX 参数由用户改 |

---

## 一、当前代码状态（已核实，不是推测）

### 1.1 构建

```
cube-cmake --build build/Debug
Flash 218,564 B (41.69%) / RAM 12,144 B (9.27%)，0 告警
```

### 1.2 已就位

| 项 | 状态 |
|---|---|
| `Attitude_GetYawRate()` | ✅ 已加。`ATTITUDE_GYRO_YAW_AXIS=2`、`_YAW_SIGN=+1`，**均未实测** |
| 遥测 12 路（含 yaw_rate / speed_l / speed_r） | ✅ 阻塞发送，54 B @ 100 Hz |
| 参数运行期副本（8 个 float） | ✅ 有真实地址，SWD 可改 |
| `User/{Inc,Src}/Demo/` | ✅ `Demo.h` + `README.md`；`DEMO_ENABLE=0`，`nm` 确认**零符号** |
| 电机死区实测 | ✅ 地面 50 / 悬空 40（PWM 计数） |
| USART3 **DMA TX 已配** | ✅ `hdma_usart3_tx`（DMA1_Stream3/CH4/Normal/Low/Byte-Byte）+ `DMA1_Stream3_IRQHandler` |

当前参数：`PID_PITCH_KP=3400`、`PID_PITCH_KD=200`、`CONTROL_DEADZONE=0`（关闭）。

### 1.3 ❗ 两个必须先处理的问题

#### 问题 A（严重）：USART3 全局中断没启用 → 遥测会静默失效

事实（全工程 grep 确认）：
- `usart.c` 里 **没有** `HAL_NVIC_EnableIRQ(USART3_IRQn)`
- `stm32f4xx_it.c` 里 **没有** `USART3_IRQHandler`
- `Balance.ioc` 里 **没有** USART3 的 NVIC 配置

**为什么致命**：HAL 的 `UART_DMATransmitCplt()`（`stm32f4xx_hal_uart.c:3015`）在 Normal 模式下
搬完数据只做两件事：

```c
ATOMIC_CLEAR_BIT(huart->Instance->CR3, USART_CR3_DMAT);
ATOMIC_SET_BIT(huart->Instance->CR1, USART_CR1_TCIE);   /* 只是打开 TC 中断 */
```

真正收尾（`HAL_UART_TxCpltCallback` + `gState` 回 `READY`）在 `USART3_IRQHandler` 里。
**没开 NVIC → 中断永不进 → `gState` 永停 `BUSY_TX` → 之后每次 `HAL_UART_Transmit_DMA`
都返回 `HAL_BUSY` → 只发出第一帧，之后遥测完全静默，且不报任何错。**

**修法（用户操作，不要改生成代码）**：CubeMX → `Connectivity → USART3 → NVIC Settings`
→ 勾 **`USART3 global interrupt`** → GENERATE CODE。
生成后自检：`usart.c` 里出现 `HAL_NVIC_EnableIRQ(USART3_IRQn)`、`it.c` 里出现 `USART3_IRQHandler`。

> ⚠️ 这正好是 `docs/11.14.md` §B1 警告过的坑，用户当时只做了 DMA 那一半。

#### 问题 B：SPI2 分频仍是 2（21 MHz）

```
Core/Src/spi.c:82   hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
Balance.ioc:552     SPI2.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_2
```

nRF24 的 SPI 上限是 10 MHz。**无线通路现在一定不通**。
CubeMX：`Connectivity → SPI2 → Clock Parameters → Prescaler` → **8**（5.25 MHz）。

---

## 二、下一步：按什么顺序做

### 路径（详见 `docs/11.14.md` §三）

```
A0（用户，5 分钟）→ F → G → H → I = 作品
```

**B / C / D / E 都不阻塞作品**，已确认：
- `target_speed` 直接用「计数/2ms」当单位，**不需要标 k_x**
- 有线 12 路遥测已够用，**不需要 DMA 才能跑**

### 立即可以做的三件事（互不依赖）

| # | 事项 | 谁做 | 入口 |
|---|---|---|---|
| 1 | 修问题 A、B（一次 CubeMX 改两处） | 用户 | 见上文 |
| 2 | **A0：yaw 轴验证** | 用户 | `docs/11.14.md` §四（6 条判据） |
| 3 | **阶段 F：`Control_Input` 扩展 + `Control_SetTarget()`** | agent | `docs/11.14.md` §F |

**F 必须写在 `Control/` 而不是 `Demo/`** —— 接口是长期能力，最终循迹层也要用。
`Demo` 只是它的临时调用者（边界定义见 `docs/11.14.md` §2.4）。

### F 之后

| 阶段 | 内容 | 备注 |
|---|---|---|
| C | `Telemetry.c:128` 把 `uart_send()` 换成 `HAL_UART_Transmit_DMA()` | **前置：问题 A 必须先修**；否则静默失效 |
| G | 速度环带目标（直行） | `docs/11.14.md` §G |
| H | 差速转向（开环 `target_steer` 优先，yaw 闭环后做） | 同上 §H |
| I | 写 `Demo.c` | 剧本表在 `Demo/README.md` §3.3 |

**I 的转向方案 A（开环差速）完全不需要 yaw** —— 所以 A0 不过也能交作品，
yaw 只用来让 8 字更对称。

---

## 三、agent 需要注意的坑（本文档独有）

1. **`HAL_UART_Transmit_DMA` 后不要立即复用 `s_frame` 缓冲**
   `Telemetry.c` 现在是单个 `static uint8_t s_frame[]` 反复填。改 DMA 后传输是异步的，
   下一帧填缓冲会与上一次 DMA 读同一块内存 → 帧内容错乱。需要**双缓冲**或
   「上一帧没发完就跳过本帧」（推荐后者，与 `Telemetry_Update` 的限频逻辑一致）。

2. **`Control_Update()` 的 `!car_run` 分支不清速度积分**
   （只有手动分支和姿态联锁分支 `reset()`）。台架推车后直接开电机开关，
   `pitch_set` 会跳 `-0.11 rad ≈ -6.3°`。**用户已知情，未要求修**，别擅自改。

3. **改 `Control_Input` 是公共结构体**
   凡用到它构造入参的地方都要同步改。目前只有 `Control.c` 的 `control_collect()` 一处，
   但 `Control_LQR.c` 也消费同一结构体。

4. **注释有一处过期**：`Control.h` 的 `CONTROL_DT_S` 写「TIM5 1kHz」、
   `Attitude.h` 写「由 TIM5 驱动」，实际都在 **TIM7**。用户未要求改，仅记录。

5. **不提交、不 commit**：用户此前明确表示「先不提交，我自己看」。本次也无提交指令。

---

## 四、硬性工作约定（违反会导致返工）

| 规则 | 说明 |
|---|---|
| **CubeMX 生成文件的外设参数由用户在 CubeMX 改** | `spi.c`/`usart.c`/`tim.c` 的时钟分频等，不要直接编辑，也不要在 USER CODE 段写覆盖补丁 |
| **注释越短越好** | 只写「频率+用途」「算法名+公式」「单位/量纲」；不写调试过程与改动历史（有 hook 强制：注释块 ≥6 行直接报错） |
| **改动保留原有风格** | 不改既有标识符、不顺手重构、只改与需求直接相关的行 |
| **新增源文件必须手动加进根 `CMakeLists.txt`** | `target_sources` |
| **不新增说明性 README** | 除非明确要求（`Demo/README.md` 是用户点名要的） |
| **构建用 `cube-cmake --build build/Debug`** | 不要手敲 cmake/ninja |

---

## 五、建议调用的 skills

| 何时 | skill | 路径 |
|---|---|---|
| **修 USART3 NVIC / SPI2 分频时核实寄存器** | `embedded-register-debug` | `c:\Users\guqian\.agents\skills\embedded-register-debug\SKILL.md` |
| **遥测收不到/乱码/静默失效** | `embedded-serial-debug` | `c:\Users\guqian\.agents\skills\embedded-serial-debug\SKILL.md` |
| **`HAL_BUSY` 静默失效、状态卡死之类疑难** | `diagnosing-bugs` | `c:\Users\guqian\.agents\skills\diagnosing-bugs\SKILL.md` |
| **进入 G/H 真正扫增益** | `embedded-hil-tuning` | `c:\Users\guqian\.agents\skills\embedded-hil-tuning\SKILL.md` |
| **烧录失败 / 探针被占** | `embedded-flash-debug` | `c:\Users\guqian\.agents\skills\embedded-flash-debug\SKILL.md` |
| **Flash/RAM 吃紧**（当前 41.7% / 9.3%，暂不需要） | `embedded-size-optimize` | `c:\Users\guqian\.agents\skills\embedded-size-optimize\SKILL.md` |

---

## 六、返工参考：这一轮已失败的方向（不要重试）

| 已试过 | 结果 | 结论位置 |
|---|---|---|
| 死区前馈 = 50 | 持续抖（比不补更抖） | `docs/死区问题-技术归纳.md` §6.1 |
| 死区前馈 = 25 | 间断抖 | 同上 |
| KD = 200（配 KP=3000/3400） | 高频振动 | 同上 |
| KD = 50 | 无法平衡 | 同上 |
| 把参数宏直接改成 `extern float` | 用户否决，要求宏留在 `.h` | 已改回「宏 + 小写运行期副本」 |

**当前结论：接受直立时的小幅振动，不再追求消除**（`docs/11.14.md` §9.1）。
只有新功能暴露稳定性问题时才回头。

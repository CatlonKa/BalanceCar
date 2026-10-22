#ifndef __IMU_TEST_H__
#define __IMU_TEST_H__

/*
 * IMU 观测测试（临时模块，不参与控制）。
 *
 * 输出 VOFA+ 的 JustFloat 协议：3 个 float（小端）+ 帧尾 00 00 80 7F，默认 100 Hz。
 *
 *   ch0  roll    rad   atan2(-acc[0], √(acc[1]²+acc[2]²))  —— 左右侧倾（右倾为正）
 *   ch1  pitch   rad   atan2(acc[1], acc[2])                —— 车头上下（前倾为正）
 *   ch2  yaw     rad   陀螺 Z 积分（右转为正）—— 会漂，只看转向方向
 *
 * 用法：
 *   main() 里 imu_init() 之后调 ImuTest_Init()（静止标定约 0.3 s）；
 *   主循环里在 imu_service() 之后调 ImuTest_Update()。
 *
 * **上电时的姿态任意**：零偏标定只看「模长≈1 g + 角速度≈0」，躺着/侧躺/倒扣都能标。
 * 但标定期间必须不动（手不要碰、车不要晃），否则会重来。
 *
 * 实测轴映射（2026-10-21 确认）：**X = 车左侧、Y = 车尾、Z = 上**（右手系）
 *   车头下压 → ch1 变大；向后 → 变小
 *   左倾     → ch0 变小；右倾 → 变大（已翻转，右倾为正）
 *   左转     → ch2 变小；右转 → 变大（已翻转，右转为正）
 *
 * ⚠️ 本车「俯仰」是绕 X 轴的角，与 Attitude.c 的 `s_pitch` 完全同一个量、同一个符号。
 */

/* 通道顺序：VOFA+ 3D 模型一般按 roll/pitch/yaw 读，顺序不符就改这里 */
#define IMU_TEST_CH_ROLL        0U
#define IMU_TEST_CH_PITCH       1U
#define IMU_TEST_CH_YAW         2U

void ImuTest_Init(void);

/* 每轮主循环调一次；内部自己按 IMU_TEST_SEND_HZ 限频 */
void ImuTest_Update(void);

#endif

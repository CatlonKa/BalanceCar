#ifndef __LIGHT_SHOW_H__
#define __LIGHT_SHOW_H__

/* 灯光+蜂鸣器表演层：主循环调用，非阻塞。
   直立（car_run 且 Attitude_IsUpright）时自动循环播放；
   停车熄灭，未直立红灯告警。 */

void LightShow_Init(void);
void LightShow_Update(void);

#endif

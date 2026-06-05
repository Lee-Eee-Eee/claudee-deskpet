/*
 * squat.h  —  深蹲检测（MMA8451Q 加速度计 + 迟滞状态机）
 */
#ifndef SQUAT_H_
#define SQUAT_H_

#include <stdint.h>

#define SQUAT_TARGET 10

void squat_reset(void);    /* 标定静止基线 + 清零计数 */
int  squat_update(void);   /* 采样一次, 返回当前计数 (0..SQUAT_TARGET) */

#endif /* SQUAT_H_ */

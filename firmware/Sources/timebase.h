/*
 * timebase.h  —  SysTick 1ms 时基
 */
#ifndef TIMEBASE_H_
#define TIMEBASE_H_

#include <stdint.h>

void     tb_init(void);                 /* 配 SysTick 为 1ms 中断 */
uint32_t tb_millis(void);               /* 自启动以来的毫秒数 */

/* 节流/帧率助手：距上次 >= period_ms 则返回 1 并更新 *last。
 * 用法: static uint32_t t=0; if (tb_due(&t,33)) { 每~30fps做一次 } */
int      tb_due(uint32_t *last, uint32_t period_ms);

#endif /* TIMEBASE_H_ */

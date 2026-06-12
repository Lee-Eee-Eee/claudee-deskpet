/*
 * led.h  —  板载 4 颗 LED 状态灯（仅用红色, 全程 PORTC, 避免与LCD/串口/蜂鸣器冲突）
 *
 *   单色靠"呼吸/快闪/进度条"的节奏区分 CC 状态, 不靠颜色。
 *   由 PIT 中断(IRQ22, ~8kHz)做软件 PWM + 呼吸相位; 主循环只调用 led_set()/led_from_cc()。
 */
#ifndef LED_H_
#define LED_H_

#include "claudee.h"

typedef enum {
    LED_OFF = 0,        /* 全灭（SLEEP / 未连接）           */
    LED_BREATHE_SLOW,   /* 缓慢微呼吸（IDLE 在线空闲）       */
    LED_BREATHE,        /* 活泼呼吸（STARTED/WORKING 干活）  */
    LED_BLINK,          /* 快闪求关注（NEEDPERM/ASK）        */
    LED_BAR,            /* 进度条: level/4 颗常亮 + 轻呼吸    */
    LED_FLASH           /* 满亮（庆祝; 主循环保持片刻再切回） */
} led_mode_t;

void led_init(void);                            /* PORTC GPIO + PIT(IRQ22) 软件 PWM */
void led_set(led_mode_t mode, uint8_t level);   /* level 仅 LED_BAR 用(0..4) */
void led_from_cc(cc_state_t cc);                /* NORMAL 屏: CC 状态 -> 呼吸/快闪 */

#endif /* LED_H_ */

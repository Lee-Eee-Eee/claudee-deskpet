/*
 * input.h  —  按键（PORTA 中断, 复用 3.2 已验证的板上写法）
 *   A = PTA14  手动唤起当前预选挑战（本地测试，不必经 CC）
 *   B = PTA16  切换预选挑战 (深蹲 <-> 跑酷) / 挑战中: 拒绝(深蹲) · 到分放行(跑酷)
 *   C = PTA17  跑酷跳跃
 *  按键为低有效(按下=低)，下降沿触发。
 */
#ifndef INPUT_H_
#define INPUT_H_

#include "claudee.h"

typedef enum { BTN_NONE = 0, BTN_A = 1, BTN_B = 2, BTN_C = 3 } btn_event_t;

void        input_init(void);
btn_event_t input_poll(void);    /* 取出并清除一个待处理按键事件 */
challenge_t input_mode(void);    /* 当前预选挑战（B 键在 ISR 里切换）*/

#endif /* INPUT_H_ */

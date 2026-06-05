/*
 * sfx.h  —  蜂鸣器音效 (TPM0_CH4 / PTC8, 复用 8.2 写法), 非阻塞
 */
#ifndef SFX_H_
#define SFX_H_

#include <stdint.h>

void sfx_init(void);
void sfx_tone(uint16_t freq, uint16_t dur_ms);  /* 起一个音, dur 后由 sfx_update 关 */
void sfx_update(void);                          /* 每循环调用：到点关音 */

/* 便捷音效 */
void sfx_tick(void);   /* 深蹲计数 */
void sfx_ok(void);     /* 放行通过 */
void sfx_deny(void);   /* 拒绝/失败 */
void sfx_jump(void);   /* 跑酷跳跃 */
void sfx_hit(void);    /* 跑酷撞击 */

#endif /* SFX_H_ */

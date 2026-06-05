/*
 * display.h  —  Claudee 状态屏渲染（四态 + 睡眠）
 */
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "claudee.h"
#include <stdint.h>

void disp_init(void);                 /* 首次：清屏 + 画 Clawd 形体 */
void disp_enter_normal(void);         /* (重)进入状态屏：重画形体并使缓存失效（挑战返回后调用）*/

/* 每帧调用：按状态画 Clawd 表情 + 顶栏 + 轮播状态词（仅重绘变化区域）*/
void disp_render(cc_state_t st, challenge_t armed, uint32_t now_ms);

/* 屏底遥测行（token/cost/ctx）；内容变化时才重画。NORMAL 屏调用。*/
void disp_telemetry(const char *s);

/* 设置情境行（挑战时的工具描述 / ASK 的问题）；ASK 屏与挑战屏显示。*/
void disp_set_context(const char *s);

/* ---- 挑战屏（NEEDPERM 时）---- */
void disp_challenge_enter(challenge_t armed);   /* 进挑战：画标题/说明/进度框 */
void disp_squat_count(int count, int target);   /* 更新深蹲计数 + 进度条 */
void disp_allowed(void);                         /* 通过：大号 ALLOWED */

#endif /* DISPLAY_H_ */

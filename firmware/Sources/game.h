/*
 * game.h  —  Clawd 跑酷（Chrome 小恐龙式）：跳过/躲过对手图标攒分，达标可放行
 *  操作: A 键(PTA14)=跳；B 键(PTA16)=到 10 分时"放行退出"。
 *  障碍: 地面图标(GPT/Gemini/Grok/Codex/Antigravity/opencode)要跳过；
 *        空中图标在头顶高度，跳起来会撞 -> 不能跳(保持地面)。
 *  到 GAME_TARGET 分: 暂停并给选择 —— B=放行退出(GAME_WIN) / A=继续(再攒 +5)。
 *  渲染: 开窗流式 blit(无残影、低延迟) + 脏矩形。
 */
#ifndef GAME_H_
#define GAME_H_

#include <stdint.h>

#define GAME_TARGET 10         /* 攒够 10 分 -> 可选择放行退出 */

typedef enum { GAME_RUN = 0, GAME_WIN = 1 } game_result_t;

void          game_reset(void);
/* 每帧调用: jump=A 键(起跳/继续), exit=B 键(到分时放行退出)。*/
game_result_t game_update(int jump_pressed, int exit_pressed, uint32_t now);
int           game_score(void);   /* 当前分数（LED 进度条用）*/

#endif /* GAME_H_ */

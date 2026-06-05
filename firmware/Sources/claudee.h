/*
 * claudee.h  —  Claudee 桌宠 全局类型/常量
 * 平台: NXP MKL25Z128 (Blazar 板) 裸机
 */
#ifndef CLAUDEE_H_
#define CLAUDEE_H_

#include <stdint.h>

/* ---------- CC 状态机（枚举值 == 串口协议字节，PC→板）---------- */
typedef enum {
    CC_SLEEP    = 0x00,  /* 未连接 / 未 /claudee 激活：Clawd 打盹 zzz   */
    CC_STARTED  = 0x01,  /* 来活了 (UserPromptSubmit)                  */
    CC_WORKING  = 0x02,  /* 忙 (PreToolUse 非受控 / PostToolUse)        */
    CC_IDLE     = 0x03,  /* 在线但空闲 (Stop / Notification idle)       */
    CC_NEEDPERM = 0x04,  /* 求批准 → 进挑战 (PermissionRequest)         */
    CC_SUBAGENT = 0x05,  /* 进阶: 子代理                               */
    CC_DONE     = 0x06,  /* 进阶: 庆祝                                 */
    CC_ASK      = 0x07   /* CC 在让你多选一 → 板提示"去终端选"(hooks 无法替选) */
} cc_state_t;

#define CC_STATE_MAX 0x07   /* 合法状态字节上限（comm RX 校验用）*/

/* 情境行最大长度（工具描述 / ASK 问题；与 protocol.py CTX_MAX 一致）*/
#define CLAUDEE_CTX_MAX 31

/* ---------- 板 → PC ---------- */
#define ACCEPT_BYTE  'A'   /* 0x41: 挑战通过 → CC 放行 */
#define DENY_BYTE    'D'   /* 0x44: 放弃 / 失败        */

/* ---------- 挑战类型（NORMAL 屏按键预选）---------- */
typedef enum { CH_SQUAT = 0, CH_GAME = 1 } challenge_t;

/* ---------- 调色板 (RGB565) ---------- */
/* Clawd 橙 #D97757 (R215 G119 B87): (26<<11)|(29<<5)|10 */
#define CLAWD_ORANGE  0xD3AA
/* 备用蓝 #4A90D9 (R74 G144 B217): (9<<11)|(36<<5)|27     */
#define CLAWD_BLUE    0x4C9B

#define COL_BG        0x0000   /* 背景：黑（贴合终端审美）*/
#define COL_TEXT      0xFFFF   /* 白字 */
#define COL_DIM       0x8430   /* 灰   */
#define COL_OK        0x07E0   /* 绿（Allowed）*/

/* 顶部状态栏底色（按状态）*/
#define BAR_SLEEP     0x2104   /* 暗灰 */
#define BAR_STARTED   0x05E0   /* 绿   */
#define BAR_WORKING   0x051F   /* 蓝   */
#define BAR_IDLE      0x6B4D   /* 中灰 */
#define BAR_NEEDPERM  0xF800   /* 红   */
#define BAR_ASK       0x780F   /* 紫（多选题）*/

#endif /* CLAUDEE_H_ */

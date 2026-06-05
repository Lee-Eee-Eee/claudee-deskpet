/*
 * game.c  —  Clawd 跑酷（NEEDPERM 且预选 GAME 时跑 / 本地 A 键唤起测试）
 *
 *  玩法: C 键跳过【地面】对手图标；【空中】图标在头顶高度 —— 跳起来会撞，要保持地面。
 *        **三条命**：撞到掉一条命(分数保留)，命掉光才把分数清零软重开。
 *        对手图标(10 个): OpenAI/Gemini/Grok/Codex/DeepSeek/Kimi/Antigravity/opencode/openclaw/Claude。
 *        攒够 GAME_TARGET(10) 分 -> 暂停给选择: B=放行退出(GAME_WIN) / C=继续(再 +5)。
 *
 *  渲染: 开窗流式(gfx_fill_fast / gfx_blit_fast, 列优先, 不镜像) —— 整块不透明重画，
 *        Clawd 每帧最后压顶 => 无残影；脏矩形局部刷新 => 顺滑不晃眼。
 */
#include "claudee.h"
#include "gfx.h"
#include "game.h"
#include "clawd_art.h"
#include "sfx.h"

/* 渲染路径开关：
 *   1 = 开窗流式(gfx_fill_fast/gfx_blit_fast) —— 比逐像素快 3-4x，刷新顺滑不晃眼。【默认】
 *       gfx_blit_fast 已修正为【列优先】流式(匹配本面板 MV=0 的 GRAM 自增方向)，不再镜像/转向。
 *   0 = 逐像素(gfx_fill/gfx_draw_indexed) —— 方向天然正确但很慢(每像素都重设光标)，仅作回退。
 *   万一上板仍发现精灵转向，先回退 0 验证，再查 gfx.c 的 gfx_blit_fast 列优先逻辑。 */
#define GFX_FAST 1
#if GFX_FAST
  #define FILL gfx_fill_fast
  #define BLIT gfx_blit_fast
#else
  #define FILL gfx_fill
  static void BLIT(uint16_t x, uint16_t y, const uint8_t *m, uint16_t w, uint16_t h,
                   uint8_t s, const uint16_t *p, uint16_t bg)
  { (void)bg; gfx_fill(x, y, (uint16_t)(w*s), (uint16_t)(h*s), bg);
    gfx_draw_indexed(x, y, m, w, h, s, p); }
#endif

/* ---- 布局/物理 ---- */
#define TOPBAR     18
#define GROUND_Y   196
#define GROUND_H   3
#define GS         2
#define CW         (CLAWD_W * GS)          /* 40 */
#define CH         (CLAWD_H * GS)          /* 24 */
#define CLAWD_X    36
#define GROUND_TOP (GROUND_Y - CH)         /* 172 站立 */
#define JUMP_V     (-12)
#define GRAV       1

#define RW 12
#define RH 12
#define RS 2
#define OBSZ      (RW * RS)                /* 24 */
#define OB_GND_Y  (GROUND_Y - OBSZ)        /* 172 地面图标 */
#define OB_AIR_Y  128                      /* 空中图标(跳起来才撞) */
#define MAXOB     3

#define COL_GROUND 0x4208
#define COL_TOPBAR 0x0008

static const uint16_t s_pal[2] = { 0, CLAWD_ORANGE };

/* ===== 对手图标 12x12 索引位图 (0=透明->黑底, 1=白卡, 2+=logo 色) ===== */
static const uint16_t PAL_OAI[]   = {0, 0xFFFF, 0x0000};                       /* 白卡, 黑环  */
static const uint16_t PAL_GEM[]   = {0, 0xFFFF, 0x2D7F};                       /* 白卡, 蓝星  */
static const uint16_t PAL_GROK[]  = {0, 0xFFFF, 0x0000};                       /* 白卡, 黑 X  */
static const uint16_t PAL_CDX[]   = {0, 0xFFFF, 0x0000};                       /* 白卡, 黑 >_ */
static const uint16_t PAL_DS[]    = {0, 0xFFFF, 0x04DF};                       /* 白卡, 深蓝鲸*/
static const uint16_t PAL_KIMI[]  = {0, 0xFFFF, 0x10BF};                       /* 白卡, navy 月*/
static const uint16_t PAL_AG[]    = {0, 0xFFFF, 0x443E, 0x354A, 0xFDE0, 0xEA06};/* 白卡,蓝绿橙红*/
static const uint16_t PAL_OC[]    = {0, 0xFFFF, 0x39E7, 0x0000};               /* 白框,灰里,黑顶*/
static const uint16_t PAL_CLAW[]  = {0, 0xFFFF, CLAWD_ORANGE};                 /* 白卡, 橙钳  */
static const uint16_t PAL_CLD[]   = {0, 0xFFFF, CLAWD_ORANGE};                 /* 白卡, 橙芒  */

static const uint8_t IC_OAI[RW*RH] = {  /* OpenAI 黑环结 */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,2,2,2,2,1,1,1,1, 1,1,1,2,1,1,1,1,2,1,1,1,
 1,1,2,1,1,1,1,1,1,2,1,1, 1,2,1,1,1,2,2,1,1,1,2,1, 1,2,1,1,2,1,1,2,1,1,2,1,
 1,2,1,1,2,1,1,2,1,1,2,1, 1,2,1,1,1,2,2,1,1,1,2,1, 1,1,2,1,1,1,1,1,1,2,1,1,
 1,1,1,2,1,1,1,1,2,1,1,1, 1,1,1,1,2,2,2,2,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_GEM[RW*RH] = {  /* Gemini 四角星 */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,1,2,2,1,1,1,1,1, 1,1,1,1,1,2,2,1,1,1,1,1,
 1,1,1,1,2,2,2,2,1,1,1,1, 1,1,2,2,2,2,2,2,2,2,1,1, 1,2,2,2,2,2,2,2,2,2,2,1,
 1,2,2,2,2,2,2,2,2,2,2,1, 1,1,2,2,2,2,2,2,2,2,1,1, 1,1,1,1,2,2,2,2,1,1,1,1,
 1,1,1,1,1,2,2,1,1,1,1,1, 1,1,1,1,1,2,2,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_GROK[RW*RH] = { /* Grok 黑 X */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,2,2,1,1,1,1,1,1,2,2,1, 1,2,2,2,1,1,1,1,2,2,2,1,
 1,1,2,2,2,1,1,2,2,2,1,1, 1,1,1,2,2,2,2,2,2,1,1,1, 1,1,1,1,2,2,2,2,1,1,1,1,
 1,1,1,1,2,2,2,2,1,1,1,1, 1,1,1,2,2,2,2,2,2,1,1,1, 1,1,2,2,2,1,1,2,2,2,1,1,
 1,2,2,2,1,1,1,1,2,2,2,1, 1,2,2,1,1,1,1,1,1,2,2,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_CDX[RW*RH] = {  /* Codex >_ */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,1,1,1,1,1,1,1,1, 1,1,2,1,1,1,1,1,1,1,1,1,
 1,1,2,2,1,1,1,1,1,1,1,1, 1,1,1,2,2,1,1,1,1,1,1,1, 1,1,1,1,2,2,1,1,1,1,1,1,
 1,1,1,2,2,1,1,1,1,1,1,1, 1,1,2,2,1,1,1,1,1,1,1,1, 1,1,2,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,2,2,2,2,1, 1,1,1,1,1,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_DS[RW*RH] = {   /* DeepSeek 蓝鲸 */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,2,1,1,
 1,1,1,1,1,1,1,1,2,1,1,1, 1,1,2,2,2,2,2,2,2,2,1,1, 1,2,2,2,2,2,2,2,2,2,2,1,
 1,2,2,2,2,2,2,2,2,2,2,1, 1,2,2,2,2,2,2,2,2,2,1,1, 1,1,2,2,2,2,2,2,2,1,1,1,
 1,1,1,2,1,1,1,2,2,2,1,1, 1,1,1,1,1,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_KIMI[RW*RH] = { /* Kimi 弯月(Moonshot) */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,2,2,2,2,1,1,1,1,1, 1,1,2,2,2,2,2,2,1,1,1,1,
 1,2,2,2,1,1,1,2,1,1,1,1, 1,2,2,1,1,1,1,1,1,1,1,1, 1,2,2,1,1,1,1,1,1,1,1,1,
 1,2,2,1,1,1,1,1,1,1,1,1, 1,2,2,2,1,1,1,2,1,1,1,1, 1,1,2,2,2,2,2,2,1,1,1,1,
 1,1,1,2,2,2,2,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_AG[RW*RH] = {   /* Antigravity 彩虹峰 */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,1,4,4,1,1,1,1,1, 1,1,1,1,1,5,5,1,1,1,1,1,
 1,1,1,1,4,4,4,4,1,1,1,1, 1,1,1,1,3,4,4,3,1,1,1,1, 1,1,1,3,3,1,1,3,3,1,1,1,
 1,1,1,2,3,1,1,3,2,1,1,1, 1,1,2,2,1,1,1,1,2,2,1,1, 1,1,2,1,1,1,1,1,1,2,1,1,
 1,2,2,1,1,1,1,1,1,2,2,1, 1,2,1,1,1,1,1,1,1,1,2,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_OC[RW*RH] = {   /* opencode 窗框 */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,3,3,3,3,3,3,1,1,1, 1,1,1,3,3,3,3,3,3,1,1,1, 1,1,1,2,2,2,2,2,2,1,1,1,
 1,1,1,2,2,2,2,2,2,1,1,1, 1,1,1,2,2,2,2,2,2,1,1,1, 1,1,1,2,2,2,2,2,2,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_CLAW[RW*RH] = { /* openclaw 橙蟹钳 */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,2,2,2,1,1,1,1,1,1, 1,1,2,2,2,2,2,1,1,1,1,1,
 1,2,2,1,1,2,2,2,1,1,1,1, 1,2,2,1,1,1,2,2,2,1,1,1, 1,2,2,1,1,1,1,2,2,2,2,1,
 1,2,2,1,1,1,1,2,2,2,2,1, 1,2,2,1,1,1,2,2,2,1,1,1, 1,2,2,1,1,2,2,2,1,1,1,1,
 1,1,2,2,2,2,2,1,1,1,1,1, 1,1,1,2,2,2,1,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };
static const uint8_t IC_CLD[RW*RH] = {  /* Claude 橙星芒(Anthropic) */
 0,1,1,1,1,1,1,1,1,1,1,0, 1,1,1,1,1,2,2,1,1,1,1,1, 1,1,2,1,1,2,2,1,1,2,1,1,
 1,1,1,2,1,2,2,1,2,1,1,1, 1,1,1,1,2,2,2,2,1,1,1,1, 1,2,2,2,2,2,2,2,2,2,2,1,
 1,1,1,1,2,2,2,2,1,1,1,1, 1,1,1,2,1,2,2,1,2,1,1,1, 1,1,2,1,1,2,2,1,1,2,1,1,
 1,1,1,1,1,2,2,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,0 };

typedef struct { const uint8_t *map; const uint16_t *pal; } rival_t;
#define NRIVAL 10
static const rival_t RIVALS[NRIVAL] = {
    { IC_OAI, PAL_OAI }, { IC_GEM, PAL_GEM }, { IC_GROK, PAL_GROK }, { IC_CDX, PAL_CDX },
    { IC_DS, PAL_DS },   { IC_KIMI, PAL_KIMI }, { IC_AG, PAL_AG },   { IC_OC, PAL_OC },
    { IC_CLAW, PAL_CLAW }, { IC_CLD, PAL_CLD },
};
static const char *const RIVAL_NM[NRIVAL] = {
    "OpenAI", "Gemini", "Grok", "Codex", "DeepSeek",
    "Kimi", "Antigravity", "opencode", "openclaw", "Claude"
};
/* 0 地面(跳过) / 1 空中(别跳)；散布几个空中障碍 */
static const uint8_t s_type_pat[NRIVAL] = { 0, 0, 1, 0, 0, 1, 0, 1, 0, 0 };
static const int     s_gaps[4]          = { 60, 80, 66, 88 };

/* ---- 状态 ---- */
static int     clawd_y, clawd_y_prev, vy, on_ground;
static int     ob_x[MAXOB], ob_xprev[MAXOB];
static uint8_t ob_on[MAXOB], ob_counted[MAXOB], ob_type[MAXOB], ob_id[MAXOB];
static int     score, score_prev, spawn_timer, spawn_n, next_choice, in_choice;
static int     lives, lives_prev;
static uint8_t gap_i;

#define LIVES_MAX 3

static int ob_y(int i) { return ob_type[i] ? OB_AIR_Y : OB_GND_Y; }

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{ return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah; }

static void erase_rect(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (w <= 0 || h <= 0 || x >= 320) return;
    if (x + w > 320) w = 320 - x;
    FILL((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, COL_BG);
}
static void draw_crab(int y)   { BLIT(CLAWD_X, (uint16_t)y, clawd_body, CLAWD_W, CLAWD_H, GS, s_pal, COL_BG); }
static void draw_ob(int i)     { BLIT((uint16_t)ob_x[i], (uint16_t)ob_y(i), RIVALS[ob_id[i]].map, RW, RH, RS, RIVALS[ob_id[i]].pal, COL_BG); }

/* 静态标签(进场/重画时画一次); 文本走逐像素较慢, 故只画一次 */
static void draw_score_label(void)
{
    FILL(0, 2, 110, 14, COL_TOPBAR);
    gfx_text(6,  2, "SCORE", 12, COL_TEXT, COL_TOPBAR);
    gfx_text(54, 2, "/10",   12, COL_DIM,  COL_TOPBAR);
}

/* 只重画两位分数(避免每次进分都慢速重绘整串 -> 消除周期性掉帧) */
static void draw_score(void)
{
    char d[3];
    if (score == score_prev) return;
    score_prev = score;
    d[0] = (char)('0' + (score / 10) % 10);
    d[1] = (char)('0' + score % 10);
    d[2] = 0;
    FILL(42, 2, 12, 14, COL_TOPBAR);
    gfx_text(42, 2, d, 12, COL_TEXT, COL_TOPBAR);
}

/* 右上角 3 颗命：橙=有, 暗=失 */
static void draw_lives(void)
{
    int k;
    if (lives == lives_prev) return;
    lives_prev = lives;
    FILL(254, 0, 66, TOPBAR, COL_TOPBAR);
    for (k = 0; k < LIVES_MAX; k++)
        FILL((uint16_t)(282 + k * 12), 5, 8, 8, (k < lives) ? CLAWD_ORANGE : 0x2945);
}

static void draw_hint(const char *h)
{
    FILL(112, 0, 140, TOPBAR, COL_TOPBAR);
    gfx_text(116, 2, h, 12, COL_DIM, COL_TOPBAR);
}

/* 重画整个游玩区(进场/撞击重置/选择关闭后)。*/
static void repaint(void)
{
    int i;
    FILL(0, 0, 320, 240, COL_BG);
    FILL(0, 0, 320, TOPBAR, COL_TOPBAR);
    FILL(0, GROUND_Y, 320, GROUND_H, COL_GROUND);
    draw_score_label(); score_prev = -1; draw_score();
    lives_prev = -1; draw_lives();
    draw_hint("C jump / dodge air");
    for (i = 0; i < MAXOB; i++) if (ob_on[i]) draw_ob(i);
    draw_crab(clawd_y);
}

void game_reset(void)
{
    int i;
    clawd_y = clawd_y_prev = GROUND_TOP; vy = 0; on_ground = 1;
    for (i = 0; i < MAXOB; i++) { ob_on[i] = 0; ob_counted[i] = 0; ob_x[i] = ob_xprev[i] = 400; }
    score = 0; score_prev = -1; spawn_timer = 26; gap_i = 0; spawn_n = 0;
    lives = LIVES_MAX; lives_prev = -1;
    next_choice = GAME_TARGET; in_choice = 0;
    repaint();
}

/* 到分选择浮层 */
static void draw_choice(void)
{
    const char *t1 = "Reached 10!";
    const char *t2 = "B = allow & exit    C = keep playing";
    FILL(20, 92, 280, 56, COL_TOPBAR);
    FILL(20, 92, 280, 2, CLAWD_ORANGE);
    FILL(20, 146, 280, 2, CLAWD_ORANGE);
    gfx_text((uint16_t)((320 - gfx_text_w(t1, 16)) / 2), 100, t1, 16, CLAWD_ORANGE, COL_TOPBAR);
    gfx_text((uint16_t)((320 - gfx_text_w(t2, 12)) / 2), 124, t2, 12, COL_TEXT, COL_TOPBAR);
}

game_result_t game_update(int jump_pressed, int exit_pressed, uint32_t now)
{
    int i, speed, hit = 0;
    (void)now;

    /* ---- 到分选择(冻结世界) ---- */
    if (in_choice) {
        if (exit_pressed) return GAME_WIN;          /* B: 放行退出 */
        if (jump_pressed) { in_choice = 0; next_choice += 5; repaint(); }  /* C: 继续 */
        return GAME_RUN;
    }

    /* ---- 跳跃 + 物理 ---- */
    if (jump_pressed && on_ground) { vy = JUMP_V; on_ground = 0; sfx_jump(); }
    vy += GRAV; clawd_y += vy;
    if (clawd_y >= GROUND_TOP) { clawd_y = GROUND_TOP; vy = 0; on_ground = 1; }

    speed = 6 + score / 3; if (speed > 9) speed = 9;

    /* ---- 生成障碍 ---- */
    if (--spawn_timer <= 0) {
        for (i = 0; i < MAXOB; i++) {
            if (!ob_on[i]) {
                ob_on[i] = 1; ob_counted[i] = 0;
                ob_type[i] = s_type_pat[spawn_n % NRIVAL];
                ob_id[i]   = (uint8_t)(spawn_n % NRIVAL);
                ob_x[i] = 320; ob_xprev[i] = 320; spawn_n++;
                break;
            }
        }
        spawn_timer = s_gaps[gap_i & 3]; gap_i++;
    }

    /* ---- 推进 + 计分 ---- */
    for (i = 0; i < MAXOB; i++) {
        if (!ob_on[i]) continue;
        ob_xprev[i] = ob_x[i]; ob_x[i] -= speed;
        if (ob_x[i] + OBSZ < 0) { erase_rect(ob_xprev[i], ob_y(i), OBSZ, OBSZ); ob_on[i] = 0; continue; }
        if (!ob_counted[i] && ob_x[i] + OBSZ < CLAWD_X) { ob_counted[i] = 1; score++; sfx_tick(); }
    }

    /* ---- 碰撞 -> 掉一条命(分数保留)；命掉光才软重开 ---- */
    for (i = 0; i < MAXOB; i++)
        if (ob_on[i] && overlap(CLAWD_X, clawd_y, CW, CH, ob_x[i], ob_y(i), OBSZ, OBSZ)) { hit = 1; break; }
    if (hit) {
        sfx_hit();
        lives--;
        /* 清掉当前障碍(免得立刻二次碰撞) + 蟹归位；只擦各自的框, 不整屏清(少闪) */
        for (i = 0; i < MAXOB; i++) {
            if (ob_on[i]) {
                int wd = OBSZ + (ob_xprev[i] - ob_x[i]);   /* 覆盖 prev..cur(障碍左移) */
                erase_rect(ob_x[i], ob_y(i), wd, OBSZ);
                ob_on[i] = 0;
            }
        }
        /* 擦掉蟹：屏上可见的是上一帧 clawd_y_prev 处那只；本帧物理已把 clawd_y 前移，
         * 两者在空中不同 -> 必须按 clawd_y_prev 擦(再擦 clawd_y 保险)，否则空中撞击留橙色残影。*/
        erase_rect(CLAWD_X, clawd_y_prev, CW, CH);
        erase_rect(CLAWD_X, clawd_y, CW, CH);
        clawd_y = clawd_y_prev = GROUND_TOP; vy = 0; on_ground = 1;
        spawn_timer = 28;                                  /* 缓冲一下再出新障碍 */
        if (lives <= 0) {                                  /* 命掉光 -> 分清零软重开(整屏重画, 少见) */
            score = 0; lives = LIVES_MAX; gap_i = 0; spawn_n = 0; next_choice = GAME_TARGET;
            repaint();
        } else {                                           /* 还有命: 局部刷新 */
            draw_lives();
            draw_crab(clawd_y);
        }
        return GAME_RUN;
    }

    /* ---- 渲染：只擦"移动让出的窄条"，其余由不透明 blit 直接覆盖 => 不重擦、不晃眼 ---- */
    for (i = 0; i < MAXOB; i++) {
        if (!ob_on[i]) continue;
        if (ob_x[i] >= 0) {                               /* 在屏内: 擦右侧让出的窄条 + 不透明重画 */
            int vac = ob_xprev[i] - ob_x[i];              /* 障碍向左移, 右边让出 vac 宽 */
            if (vac > 0) erase_rect(ob_x[i] + OBSZ, ob_y(i), vac, OBSZ);
            draw_ob(i);
        } else {                                          /* 已滑出左缘不再画 -> 整框擦掉旧位 */
            erase_rect(ob_xprev[i], ob_y(i), OBSZ, OBSZ);
        }
    }
    if (clawd_y != clawd_y_prev) {                         /* 蟹竖直移动: 只擦让出的横条 */
        int dy = clawd_y - clawd_y_prev;
        if (dy > 0) erase_rect(CLAWD_X, clawd_y_prev, CW, dy);    /* 下落: 顶部让出 dy */
        else        erase_rect(CLAWD_X, clawd_y + CH, CW, -dy);   /* 上升: 底部让出 -dy */
        clawd_y_prev = clawd_y;
    }
    draw_crab(clawd_y);                                          /* 每帧不透明重画, 压最上层 */
    draw_score();

    if (score >= next_choice) { in_choice = 1; draw_choice(); }  /* 到分 -> 选择 */
    return GAME_RUN;
}

int game_score(void) { return score; }

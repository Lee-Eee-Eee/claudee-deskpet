/*
 * display.c  —  Claudee 状态屏（320x240 横屏）
 *  布局:
 *    顶栏  y0..28      : 底色随状态; 左 "Claudee"  右 状态标签
 *    Clawd y44..156    : 18x14 形体 scale=8 -> 144x112, 居中 x=88
 *    星芒  右上         : Claude ✶ 脉冲(收缩放大)
 *    状态词 y176        : 居中轮播(CC 俏皮词), ~2.5s 一换(放慢)
 *    遥测  y202         : token/cost/ctx
 *    徽章  y222         : 预选挑战 SQUAT/GAME
 *  灵动来源(廉价局部重绘, 不整屏重画形体): 眨眼/表情 + 两侧手挥动 + 星芒脉冲。
 */
#include "Blazar_TFTLCD.h"
#include "claudee.h"
#include "gfx.h"
#include "display.h"
#include "clawd_art.h"

/* ---- 布局常量 ---- */
#define SCALE     8
#define SPR_X     80
#define SPR_Y     44
#define SPR_W     (CLAWD_W * SCALE)   /* 160 */
#define SPR_H     (CLAWD_H * SCALE)   /* 96  */
#define BAR_H     28
#define WORD_Y    168
#define WORD_H    20
#define ASKQ_Y    186                 /* ASK 屏：问题行 */
#define TELE_Y    200
#define BADGE_Y   220
#define WORD_MS   2500u               /* 状态词轮播周期(放慢) */

/* 眼睛屏幕坐标/尺寸 */
#define EYE_L_X   (SPR_X + CLAWD_EYE_L_X * SCALE)   /* 120 */
#define EYE_R_X   (SPR_X + CLAWD_EYE_R_X * SCALE)   /* 184 */
#define EYE_Y     (SPR_Y + CLAWD_EYE_Y * SCALE)     /* 68  */
#define EYE_PW    (CLAWD_EYE_W * SCALE)             /* 16  */
#define EYE_PH    (CLAWD_EYE_H * SCALE)             /* 16  */

/* 两侧手(钳)屏幕坐标 */
#define HAND_L_X  (SPR_X + CLAWD_HAND_L_X * SCALE)  /* 88  */
#define HAND_R_X  (SPR_X + CLAWD_HAND_R_X * SCALE)  /* 224 */
#define HAND_W    (CLAWD_HAND_W * SCALE)            /* 16  */
#define HAND_Y    (SPR_Y + CLAWD_HAND_Y * SCALE)    /* 92  */
#define HAND_PH   (CLAWD_HAND_H * SCALE)            /* 16  */
#define HAND_CLR_Y (HAND_Y - 8)                      /* 挥动擦除范围上沿 */
#define HAND_CLR_H 32

/* Claude 星芒(脉冲)位置 —— 放在更宽的 Clawd(80..240)右侧空白处 */
#define SPK_CX    284
#define SPK_CY    58
#define SPK_BOX   40                  /* 擦除盒边长(以 CX/CY 为中心) */

/* zzz */
#define ZZZ_X     (SPR_X + SPR_W - 6)
#define ZZZ_Y     (SPR_Y + 4)
#define ZZZ_W     54

static const uint16_t s_pal[2] = { 0, CLAWD_ORANGE };

/* 眼神表情 */
typedef enum { EX_NORMAL, EX_WIDE, EX_BLINK, EX_HAPPY, EX_FROWN } eye_expr_t;

/* ---- 状态词库（CC 俏皮 spinner 词）---- */
static const char *const W_WORK[] = {
    "Percolating", "Reticulating", "Conjuring", "Noodling",
    "Vibing", "Cogitating", "Schlepping"
};
static const char *const W_IDLE[]  = { "Moseying", "Snoozing", "Vibing", "Pondering" };
static const char *const W_START[] = { "On it!", "Here we go" };
static const char *const W_PERM[]  = { "Allow me?", "Tap to allow" };

static const char *word_at(cc_state_t st, int i)
{
    switch (st) {
        case CC_WORKING:  return W_WORK[i];
        case CC_IDLE:     return W_IDLE[i];
        case CC_STARTED:  return W_START[i];
        case CC_NEEDPERM: return W_PERM[i];
        default:          return 0;
    }
}
static int word_count(cc_state_t st)
{
    switch (st) {
        case CC_WORKING:  return 7;
        case CC_IDLE:     return 4;
        case CC_STARTED:  return 2;
        case CC_NEEDPERM: return 2;
        default:          return 0;
    }
}
static const char *state_label(cc_state_t st)
{
    switch (st) {
        case CC_SLEEP:    return "SLEEP";
        case CC_STARTED:  return "STARTED";
        case CC_WORKING:  return "WORKING";
        case CC_IDLE:     return "IDLE";
        case CC_NEEDPERM: return "PERMISSION";
        case CC_SUBAGENT: return "SUBAGENT";
        case CC_DONE:     return "DONE";
        case CC_ASK:      return "CHOOSE";
        default:          return "";
    }
}
static uint16_t state_bar(cc_state_t st)
{
    switch (st) {
        case CC_SLEEP:    return BAR_SLEEP;
        case CC_STARTED:  return BAR_STARTED;
        case CC_WORKING:  return BAR_WORKING;
        case CC_IDLE:     return BAR_IDLE;
        case CC_NEEDPERM: return BAR_NEEDPERM;
        case CC_ASK:      return BAR_ASK;
        default:          return BAR_IDLE;
    }
}
/* 各状态的"基础"表情 */
static eye_expr_t base_expr(cc_state_t st)
{
    switch (st) {
        case CC_STARTED:  return EX_HAPPY;
        case CC_NEEDPERM: return EX_WIDE;
        case CC_ASK:      return EX_WIDE;
        case CC_DONE:     return EX_HAPPY;
        case CC_SLEEP:    return EX_BLINK;
        default:          return EX_NORMAL;   /* WORKING / IDLE */
    }
}

/* ---- 缓存（增量重绘）---- */
static cc_state_t  s_last_st    = (cc_state_t)0xFF;
static int         s_last_word  = -1;
static eye_expr_t  s_eye        = (eye_expr_t)0xFF;
static int         s_spk        = -1;   /* 上次星芒臂长 */
static int         s_claw       = -1;   /* 上次手挥相位 */
static int         s_zzz        = -1;
static challenge_t s_last_armed = (challenge_t)0xFF;

static void draw_bar(cc_state_t st)
{
    uint16_t bg = state_bar(st);
    const char *lab = state_label(st);
    gfx_fill(0, 0, 320, BAR_H, bg);
    gfx_text(6, 7, "Claudee", 16, COL_TEXT, bg);
    {
        uint16_t w = gfx_text_w(lab, 16);
        gfx_text((uint16_t)(320 - 6 - w), 7, lab, 16, COL_TEXT, bg);
    }
}

/* 在一个眼眶里画指定表情(先用身体橙擦底) */
static void draw_one_eye(uint16_t x, eye_expr_t e, int is_left)
{
    gfx_fill(x, EYE_Y, EYE_PW, EYE_PH, CLAWD_ORANGE);     /* 擦眼眶 */
    switch (e) {
        case EX_WIDE:                                      /* 瞪大: 整块黑 */
            gfx_fill(x, EYE_Y, EYE_PW, EYE_PH, 0x0000);
            break;
        case EX_BLINK:                                     /* 闭眼: 横线 */
            gfx_fill(x, EYE_Y + EYE_PH / 2 - 1, EYE_PW, 3, 0x0000);
            break;
        case EX_HAPPY:                                     /* 笑眼: ^ 形(随眼框大小) */
            gfx_fill(x + 2,                EYE_Y + EYE_PH - 9, 6, 3, 0x0000);
            gfx_fill(x + EYE_PW / 2 - 3,   EYE_Y + 4,          6, 3, 0x0000);
            gfx_fill(x + EYE_PW - 8,       EYE_Y + EYE_PH - 9, 6, 3, 0x0000);
            break;
        case EX_FROWN:                                     /* 抓狂: 左> 右< */
            gfx_text(x + 3, EYE_Y, is_left ? ">" : "<", 16, 0x0000, CLAWD_ORANGE);
            break;
        case EX_NORMAL:                                    /* 普通: 黑瞳+高光 */
        default:
            gfx_fill(x + 2, EYE_Y + 2, EYE_PW - 4, EYE_PH - 4, 0x0000);
            gfx_fill(x + 3, EYE_Y + 3, 3, 3, 0xFFFF);
            break;
    }
}
static void draw_eyes(eye_expr_t e)
{
    draw_one_eye(EYE_L_X, e, 1);
    draw_one_eye(EYE_R_X, e, 0);
}

/* 两侧手挥动: phase 0/1 决定抬起量 */
static void draw_claws(int phase, int raised)
{
    int off = raised ? 8 : (phase ? 6 : 0);
    gfx_fill(HAND_L_X, HAND_CLR_Y, HAND_W, HAND_CLR_H, COL_BG);
    gfx_fill(HAND_R_X, HAND_CLR_Y, HAND_W, HAND_CLR_H, COL_BG);
    gfx_fill(HAND_L_X, (uint16_t)(HAND_Y - off), HAND_W, HAND_PH, CLAWD_ORANGE);
    gfx_fill(HAND_R_X, (uint16_t)(HAND_Y - off), HAND_W, HAND_PH, CLAWD_ORANGE);
}

/* Claude 星芒: 8 道光 + 中心, 臂长 L 脉冲 */
static void draw_sparkle(int L)
{
    uint16_t cx = SPK_CX, cy = SPK_CY;
    int d = (L * 7) / 10;            /* 斜向稍短 */
    int k;
    gfx_fill((uint16_t)(cx - SPK_BOX / 2), (uint16_t)(cy - SPK_BOX / 2),
             SPK_BOX, SPK_BOX, COL_BG);                       /* 擦盒 */
    /* 正交四道 */
    gfx_fill((uint16_t)(cx - 1), (uint16_t)(cy - L), 3, (uint16_t)L, CLAWD_ORANGE);
    gfx_fill((uint16_t)(cx - 1), cy,                3, (uint16_t)L, CLAWD_ORANGE);
    gfx_fill((uint16_t)(cx - L), (uint16_t)(cy - 1), (uint16_t)L, 3, CLAWD_ORANGE);
    gfx_fill(cx,                (uint16_t)(cy - 1), (uint16_t)L, 3, CLAWD_ORANGE);
    /* 斜向四道(阶梯小块) */
    for (k = 2; k < d; k += 2) {
        gfx_fill((uint16_t)(cx + k - 1), (uint16_t)(cy + k - 1), 2, 2, CLAWD_ORANGE);
        gfx_fill((uint16_t)(cx - k - 1), (uint16_t)(cy + k - 1), 2, 2, CLAWD_ORANGE);
        gfx_fill((uint16_t)(cx + k - 1), (uint16_t)(cy - k - 1), 2, 2, CLAWD_ORANGE);
        gfx_fill((uint16_t)(cx - k - 1), (uint16_t)(cy - k - 1), 2, 2, CLAWD_ORANGE);
    }
    gfx_fill((uint16_t)(cx - 2), (uint16_t)(cy - 2), 4, 4, CLAWD_ORANGE);  /* 中心 */
}

static void draw_word(cc_state_t st, int idx)
{
    gfx_fill(0, WORD_Y - 2, 320, WORD_H, COL_BG);
    if (st == CC_SLEEP) {
        const char *hint = "/claudee to wake";
        gfx_text((uint16_t)((320 - gfx_text_w(hint, 12)) / 2), WORD_Y + 2, hint, 12, COL_DIM, COL_BG);
        return;
    }
    {
        const char *wd = word_at(st, idx);
        if (!wd) return;
        {
            uint16_t w = gfx_text_w(wd, 16);
            uint16_t col = (st == CC_NEEDPERM) ? (uint16_t)0xF800 : CLAWD_ORANGE;
            gfx_text((uint16_t)((320 - w) / 2), WORD_Y, wd, 16, col, COL_BG);
        }
    }
}

static void draw_zzz(int idx)
{
    static const char *z[4] = { "", "z", "z z", "z z z" };
    gfx_fill(ZZZ_X, ZZZ_Y, ZZZ_W, 18, COL_BG);
    gfx_text(ZZZ_X, ZZZ_Y, z[idx & 3], 16, COL_DIM, COL_BG);
}

static void draw_badge(challenge_t armed)
{
    gfx_fill(0, BADGE_Y, 200, 14, COL_BG);
    gfx_text(6, BADGE_Y, "Challenge:", 12, COL_DIM, COL_BG);
    gfx_text((uint16_t)(6 + gfx_text_w("Challenge: ", 12)), BADGE_Y,
             (armed == CH_GAME) ? "GAME" : "SQUAT", 12, CLAWD_ORANGE, COL_BG);
}

/* ---- 遥测行（token/cost/ctx）---- */
static char s_tele_cache[40] = {0};
/* ---- 情境行（挑战工具描述 / ASK 问题）---- */
static char s_ctx[CLAUDEE_CTX_MAX + 1] = {0};
static int  s_ctx_dirty = 0;

void disp_set_context(const char *s)
{
    int i;
    if (!s) return;
    for (i = 0; i < CLAUDEE_CTX_MAX && s[i]; i++) s_ctx[i] = s[i];
    s_ctx[i] = 0;
    s_ctx_dirty = 1;
}

static int tele_eq(const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}
static void draw_tele_line(const char *s)
{
    gfx_fill(0, TELE_Y, 320, 14, COL_BG);
    if (s && s[0]) {
        gfx_text((uint16_t)((320 - gfx_text_w(s, 12)) / 2), TELE_Y, s, 12, COL_DIM, COL_BG);
    }
}
void disp_telemetry(const char *s)
{
    int i;
    if (!s) return;
    if (tele_eq(s, s_tele_cache)) return;
    for (i = 0; i < 39 && s[i]; i++) s_tele_cache[i] = s[i];
    s_tele_cache[i] = 0;
    draw_tele_line(s_tele_cache);
}

/* 画静态 Clawd(身体+黑眼), 供挑战屏/通过屏复用 */
static void draw_clawd_static(uint16_t x, uint16_t y, uint8_t scale, eye_expr_t e)
{
    uint16_t elx = (uint16_t)(x + CLAWD_EYE_L_X * scale);
    uint16_t erx = (uint16_t)(x + CLAWD_EYE_R_X * scale);
    uint16_t ey  = (uint16_t)(y + CLAWD_EYE_Y * scale);
    uint16_t ew  = (uint16_t)(CLAWD_EYE_W * scale);
    gfx_draw_indexed(x, y, clawd_body, CLAWD_W, CLAWD_H, scale, s_pal);
    if (e == EX_WIDE) {
        gfx_fill(elx, ey, ew, ew, 0x0000);
        gfx_fill(erx, ey, ew, ew, 0x0000);
    } else {
        gfx_fill((uint16_t)(elx + 1), (uint16_t)(ey + 1), (uint16_t)(ew - 2), (uint16_t)(ew - 2), 0x0000);
        gfx_fill((uint16_t)(erx + 1), (uint16_t)(ey + 1), (uint16_t)(ew - 2), (uint16_t)(ew - 2), 0x0000);
    }
}

void disp_enter_normal(void)
{
    gfx_fill(0, BAR_H, 320, (uint16_t)(240 - BAR_H), COL_BG);
    gfx_draw_indexed(SPR_X, SPR_Y, clawd_body, CLAWD_W, CLAWD_H, SCALE, s_pal);
    s_last_st = (cc_state_t)0xFF; s_last_word = -1; s_eye = (eye_expr_t)0xFF;
    s_spk = -1; s_claw = -1; s_zzz = -1; s_last_armed = (challenge_t)0xFF;
    if (s_tele_cache[0]) draw_tele_line(s_tele_cache);
}

void disp_init(void)
{
    LCD_Clear(COL_BG);
    disp_enter_normal();
}

void disp_render(cc_state_t st, challenge_t armed, uint32_t now)
{
    eye_expr_t want;

    if (st != s_last_st) {
        if (s_last_st == CC_ASK) gfx_fill(0, WORD_Y - 2, 320, 34, COL_BG);  /* 清 ASK 残留 */
        draw_bar(st);
        s_last_st = st; s_last_word = -1; s_eye = (eye_expr_t)0xFF;
        s_spk = -1; s_claw = -1; s_zzz = -1;
        if (st == CC_SLEEP) { s_tele_cache[0] = 0; gfx_fill(0, TELE_Y, 320, 14, COL_BG); }
    }

    if (armed != s_last_armed) { draw_badge(armed); s_last_armed = armed; }

    if (st == CC_SLEEP) {
        /* 睡: 闭眼 + 收起手 + zzz, 不轮播词/星芒 */
        if (s_eye != EX_BLINK) { draw_eyes(EX_BLINK); s_eye = EX_BLINK; }
        if (s_claw != 0) { draw_claws(0, 0); s_claw = 0; }
        if (s_spk != 0) {            /* 擦掉星芒 */
            gfx_fill((uint16_t)(SPK_CX - SPK_BOX / 2), (uint16_t)(SPK_CY - SPK_BOX / 2),
                     SPK_BOX, SPK_BOX, COL_BG);
            s_spk = 0;
        }
        if (s_last_word != -2) { draw_word(st, 0); s_last_word = -2; }
        { int zi = (int)((now / 500) % 4); if (zi != s_zzz) { draw_zzz(zi); s_zzz = zi; } }
        return;
    }

    /* CC 在让你多选一：板上只能提示去终端选(hooks 无法替选) + 显示问题 */
    if (st == CC_ASK) {
        if (s_eye != EX_WIDE) { draw_eyes(EX_WIDE); s_eye = EX_WIDE; }
        if (s_last_word != -3 || s_ctx_dirty) {
            const char *h = "Choose in terminal";
            gfx_fill(0, WORD_Y - 2, 320, 34, COL_BG);
            gfx_text((uint16_t)((320 - gfx_text_w(h, 16)) / 2), WORD_Y, h, 16, COL_TEXT, COL_BG);
            if (s_ctx[0])
                gfx_text((uint16_t)((320 - gfx_text_w(s_ctx, 12)) / 2), ASKQ_Y, s_ctx, 12, COL_DIM, COL_BG);
            s_last_word = -3; s_ctx_dirty = 0;
        }
        { int phase = (int)((now / 500) % 2);
          if (phase != s_claw) { draw_claws(phase & 1, 0); s_claw = phase; } }
        { int t = (int)((now / 130) % 16); int L = 6 + (t < 8 ? t : 16 - t);
          if (L != s_spk) { draw_sparkle(L); s_spk = L; } }
        return;
    }

    /* 状态词轮播(放慢到 2.5s) */
    {
        int n = word_count(st);
        if (n > 0) {
            int idx = (int)((now / WORD_MS) % (uint32_t)n);
            if (idx != s_last_word) { draw_word(st, idx); s_last_word = idx; }
        }
    }

    /* 眼神: 基础表情 + 每 4s 眨眼 ~150ms */
    want = base_expr(st);
    if (want != EX_BLINK && (now % 4000u) < 150u) want = EX_BLINK;
    if (want != s_eye) { draw_eyes(want); s_eye = want; }

    /* 两侧手挥动: WORKING 快(220ms), 其它慢(700ms); NEEDPERM 举起 */
    {
        int raised = (st == CC_NEEDPERM);
        uint32_t per = (st == CC_WORKING) ? 220u : 700u;
        int phase = raised ? 2 : (int)((now / per) % 2);
        if (phase != s_claw) { draw_claws(phase & 1, raised); s_claw = phase; }
    }

    /* Claude 星芒脉冲(收缩放大): 臂长 6..14 三角波, ~130ms 步进 */
    {
        int t = (int)((now / 130) % 16);
        int L = 6 + (t < 8 ? t : 16 - t);
        if (L != s_spk) { draw_sparkle(L); s_spk = L; }
    }
}

/* ====================== 挑战屏（NEEDPERM）====================== */
#define PBAR_X 40
#define PBAR_Y 150
#define PBAR_W 240
#define PBAR_H 24

static int s_sq_last = -1;

static void u2s(char *b, int v)
{
    if (v < 0) v = 0;
    if (v >= 10) { b[0] = (char)('0' + (v / 10) % 10); b[1] = (char)('0' + v % 10); b[2] = 0; }
    else         { b[0] = (char)('0' + v); b[1] = 0; }
}

void disp_squat_count(int count, int target)
{
    char line[24]; int p = 0; const char *s; char nb[4]; char *q;
    if (count == s_sq_last) return;
    s_sq_last = count;
    s = "SQUATS  "; while (*s) line[p++] = *s++;
    u2s(nb, count);  q = nb; while (*q) line[p++] = *q++;
    line[p++] = '/';
    u2s(nb, target); q = nb; while (*q) line[p++] = *q++;
    line[p] = 0;
    gfx_fill(0, 112, 320, 18, COL_BG);
    gfx_text((uint16_t)((320 - gfx_text_w(line, 16)) / 2), 112, line, 16, COL_TEXT, COL_BG);
    {
        int w = (PBAR_W * count) / target;
        if (w < 0) w = 0;
        if (w > PBAR_W) w = PBAR_W;
        gfx_fill(PBAR_X, PBAR_Y, PBAR_W, PBAR_H, COL_BG);
        if (w > 0) gfx_fill(PBAR_X, PBAR_Y, (uint16_t)w, PBAR_H, CLAWD_ORANGE);
    }
}

void disp_challenge_enter(challenge_t armed)
{
    gfx_fill(0, 0, 320, BAR_H, BAR_NEEDPERM);
    gfx_text(6, 7, "PERMISSION", 16, COL_TEXT, BAR_NEEDPERM);
    gfx_fill(0, BAR_H, 320, (uint16_t)(240 - BAR_H), COL_BG);
    draw_clawd_static((uint16_t)((320 - CLAWD_W * 6) / 2), 36, 6, EX_WIDE);   /* 警觉小 Clawd */
    if (armed == CH_SQUAT) {
        const char *t = "Do 10 squats to allow";
        gfx_text((uint16_t)((320 - gfx_text_w(t, 12)) / 2), 132, t, 12, COL_TEXT, COL_BG);
        gfx_fill(PBAR_X - 2, PBAR_Y - 2, PBAR_W + 4, 2, COL_DIM);
        gfx_fill(PBAR_X - 2, PBAR_Y + PBAR_H, PBAR_W + 4, 2, COL_DIM);
        gfx_fill(PBAR_X - 2, PBAR_Y - 2, 2, PBAR_H + 4, COL_DIM);
        gfx_fill(PBAR_X + PBAR_W, PBAR_Y - 2, 2, PBAR_H + 4, COL_DIM);
        s_sq_last = -1;
        disp_squat_count(0, 10);
        if (s_ctx[0])                         /* CC 要跑什么 */
            gfx_text((uint16_t)((320 - gfx_text_w(s_ctx, 12)) / 2), 180, s_ctx, 12, CLAWD_ORANGE, COL_BG);
        {
            const char *d = "B = deny";
            gfx_text((uint16_t)((320 - gfx_text_w(d, 12)) / 2), 200, d, 12, COL_DIM, COL_BG);
        }
    } else {
        const char *t = "GAME mode - press A to jump";
        gfx_text((uint16_t)((320 - gfx_text_w(t, 12)) / 2), 140, t, 12, CLAWD_ORANGE, COL_BG);
    }
}

void disp_allowed(void)
{
    const char *t = "Approved!";
    gfx_fill(0, 0, 320, BAR_H, COL_OK);
    gfx_text(6, 7, "ALLOWED", 16, COL_TEXT, COL_OK);
    gfx_fill(0, BAR_H, 320, (uint16_t)(240 - BAR_H), COL_BG);
    gfx_text((uint16_t)((320 - gfx_text_w(t, 16)) / 2), 96, t, 16, COL_OK, COL_BG);
    draw_clawd_static((uint16_t)((320 - CLAWD_W * 8) / 2), 120, 8, EX_HAPPY);
}

/*
 * main.c  —  Claudee 固件顶层 (P1+P2+P3)
 *
 *  状态机:
 *    NORMAL : 按 comm_state() 画 Clawd(SLEEP/STARTED/WORKING/IDLE) + 轮播状态词。
 *             B 键(ISR 内)切换预选挑战(深蹲/跑酷)。
 *    WAIT   : 当 CC 进入 NEEDPERM 且本轮未处理时进入；跑预选挑战：
 *               深蹲: squat_update() 计数, 满 10 -> comm_send_accept()
 *               跑酷(P4 前占位): 按 A -> comm_send_accept()
 *             通过 -> 显示 ALLOWED -> 回 NORMAL。
 *             若 CC 离开 NEEDPERM(撤销/超时/睡眠) -> 放弃挑战回 NORMAL。
 *  init: gpio_enable_port -> tb_init -> LCD_Init -> MMA8451Q_Init -> comm_init
 *        -> input_init -> sfx_init -> 开中断 -> disp_init
 */
#include "derivative.h"
#include "claudee.h"
#include "Blazar_TFTLCD.h"
#include "KL2x_gpio.h"
#include "MMA8451Q.h"
#include "timebase.h"
#include "comm.h"
#include "display.h"
#include "input.h"
#include "sfx.h"
#include "squat.h"
#include "game.h"
#include "led.h"

/* 阻塞等待 ms，但持续 service 蜂鸣器（让短音效正常收尾）*/
static void hold_ms(uint32_t ms)
{
    uint32_t end = tb_millis() + ms;
    while ((int32_t)(tb_millis() - end) < 0) {
        sfx_update();
    }
}

int main(void)
{
    gpio_enable_port();
    tb_init();
    LCD_Init();
    MMA8451Q_Init();
    comm_init();
    input_init();
    sfx_init();
    led_init();
    __asm volatile ("cpsie i");      /* 全局开中断 */
    disp_init();

    {
        enum { M_NORMAL, M_WAIT } mode = M_NORMAL;
        int         served = 0;       /* 本轮 NEEDPERM 是否已处理（防重入）*/
        challenge_t active = CH_SQUAT;/* 进挑战时锁定的类型 */
        int         sq_last = -1;
        int         jump_pending = 0; /* 跑酷：把按键跳跃 latch 到下一帧 */
        int         exit_pending = 0; /* 跑酷：到分时 B=放行退出 */
        int         manual = 0;       /* 本次挑战是否按 A 手动唤起(本地测试,不回传 accept) */
        uint32_t    t_render = 0, t_squat = 0, t_game = 0;

        for (;;) {
            uint32_t    now   = tb_millis();
            cc_state_t  cc    = comm_state();
            challenge_t armed = input_mode();
            btn_event_t btn   = input_poll();

            /* 新情境行(工具描述/ASK 问题) -> 交给 display 缓存(ASK 屏 & 挑战屏用) */
            { char cb[CLAUDEE_CTX_MAX + 1];
              if (comm_get_context(cb, (int)sizeof cb)) disp_set_context(cb); }

            if (mode == M_NORMAL) {
                if (tb_due(&t_render, 30)) {
                    disp_render(cc, armed, now);
                    led_from_cc(cc);          /* LED 随 CC 状态呼吸/快闪 */
                    /* 屏底刷新 token/花费/上下文(来自 statusLine 遥测)；睡眠时丢弃 */
                    { char tb[40];
                      if (comm_get_telemetry(tb, (int)sizeof tb) && cc != CC_SLEEP)
                          disp_telemetry(tb); }
                }
                if (cc != CC_NEEDPERM) {
                    served = 0;                       /* 离开 NEEDPERM -> 解锁 */
                }
                /* 进挑战：CC 真请求(NEEDPERM) 或 按 A 手动唤起(本地测试) */
                if ((cc == CC_NEEDPERM && !served) || btn == BTN_A) {
                    manual = (btn == BTN_A) && !(cc == CC_NEEDPERM && !served);
                    active = armed;
                    mode = M_WAIT;
                    led_set(LED_BAR, 0);              /* 进度条起步(空) */
                    if (active == CH_SQUAT) {
                        disp_challenge_enter(active);
                        squat_reset(); sq_last = -1;
                    } else {                          /* 跑酷：自建场景 */
                        game_reset(); jump_pending = 0; exit_pending = 0;
                    }
                }
            } else { /* M_WAIT */
                if (!manual && cc != CC_NEEDPERM) {       /* 仅 CC 触发的会被撤销/超时打断 */
                    mode = M_NORMAL;
                    served = 0;
                    disp_enter_normal();
                } else if (active == CH_SQUAT) {
                    if (btn == BTN_B) {                   /* 拒绝/退出 */
                        if (!manual) { comm_send_deny(); sfx_deny(); served = 1; }
                        mode = M_NORMAL; manual = 0;
                        disp_enter_normal();
                    } else if (tb_due(&t_squat, 20)) {    /* ~50Hz 采样 */
                        int c = squat_update();
                        if (c != sq_last) {
                            disp_squat_count(c, SQUAT_TARGET);
                            led_set(LED_BAR, (uint8_t)((c >= SQUAT_TARGET) ? 4 : (c * 4) / SQUAT_TARGET));
                            if (c > sq_last && c > 0) sfx_tick();
                            sq_last = c;
                        }
                        if (c >= SQUAT_TARGET) {          /* 完成 */
                            sfx_ok();
                            led_set(LED_FLASH, 0);
                            if (!manual) { comm_send_accept(); disp_allowed(); hold_ms(1300); served = 1; }
                            mode = M_NORMAL; manual = 0;
                            disp_enter_normal();
                        }
                    }
                } else { /* CH_GAME 跑酷：C 跳跃，B 到分放行(CC)/手动测试退出 */
                    if (btn == BTN_C) jump_pending = 1;   /* C 键跳跃 */
                    if (btn == BTN_B) {
                        if (manual) { mode = M_NORMAL; manual = 0; disp_enter_normal(); }
                        else exit_pending = 1;            /* CC 模式：到分放行 */
                    }
                    if (mode == M_WAIT && tb_due(&t_game, 22)) {   /* ~45fps(开窗流式够快) */
                        game_result_t gr = game_update(jump_pending, exit_pending, now);
                        { int s = game_score();
                          led_set(LED_BAR, (uint8_t)((s >= GAME_TARGET) ? 4 : (s * 4) / GAME_TARGET)); }
                        if (gr == GAME_WIN) {
                            sfx_ok();
                            led_set(LED_FLASH, 0);
                            if (!manual) { comm_send_accept(); disp_allowed(); hold_ms(1300); served = 1; }
                            mode = M_NORMAL; manual = 0;
                            disp_enter_normal();
                        }
                        jump_pending = 0;
                        exit_pending = 0;
                    }
                }
            }
            sfx_update();
        }
    }
}

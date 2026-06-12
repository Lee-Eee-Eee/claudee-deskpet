/*
 * comm.c  —  UART1 串口通信
 *  - NVIC 使能 + UART1_IRQHandler，做中断式 RX（不阻塞主循环/动画）。
 *  - UART1 = IRQ13（按 kinetis_sysinit.c 向量表核实）。
 *  - 看门狗：>3s 没收到任何字节 → 视为断连(CC_SLEEP)，覆盖拔线/CC 退出/桥崩溃。
 *
 *  RX 解析三类:
 *    单字节状态码 0x00..0x07 (含 ASK=0x07)            -> s_state
 *    遥测帧  0xFE <ASCII...> 0x0A (token/cost/ctx)     -> comm_get_telemetry
 *    情境帧  0xFC <ASCII...> 0x0A (工具描述 / ASK 问题) -> comm_get_context
 */
#include "derivative.h"
#include "claudee.h"
#include "comm.h"
#include "timebase.h"
#include "UART.h"          /* 1.2: UART1_Init / UART1_PutChar */

#define LINK_TIMEOUT_MS  3000u

#define TELE_SOF   0xFEu
#define CTX_SOF    0xFCu
#define FRAME_EOL  0x0Au
#define FRAME_MAX  CLAUDEE_CTX_MAX     /* 31，遥测/情境共用 */

static volatile cc_state_t s_state   = CC_SLEEP;
static volatile uint32_t   s_last_rx = 0;

/* 两路帧缓冲（ISR 收，主循环取）*/
static volatile char    s_tele[FRAME_MAX + 1] = {0};
static volatile char    s_ctx[FRAME_MAX + 1]  = {0};
static volatile uint8_t s_tele_new = 0;
static volatile uint8_t s_ctx_new  = 0;

/* 帧收集态: 0=无, 1=遥测(0xFE), 2=情境(0xFC) */
static char    s_fbuf[FRAME_MAX + 1];
static uint8_t s_flen  = 0;
static uint8_t s_fkind = 0;

static void commit_frame(void)
{
    uint8_t i;
    volatile char *dst = (s_fkind == 1) ? s_tele : s_ctx;
    s_fbuf[s_flen] = 0;
    for (i = 0; i <= s_flen; i++) dst[i] = s_fbuf[i];
    if (s_fkind == 1) s_tele_new = 1; else s_ctx_new = 1;
}

/* 覆盖 weak UART1_IRQHandler */
void UART1_IRQHandler(void)
{
    if (UART1_S1 & 0x20) {                       /* RDRF：收到一字节 */
        uint8_t b = (uint8_t)UART1_D;            /* 读 D 清 RDRF */
        s_last_rx = tb_millis();
        if (s_fkind) {                            /* 正在收一帧 */
            if (b == FRAME_EOL) { commit_frame(); s_fkind = 0; }
            else if (b >= 0x20 && s_flen < FRAME_MAX) s_fbuf[s_flen++] = (char)b;
            /* 收集态下其它字节忽略，直到 EOL */
        } else if (b == TELE_SOF) {
            s_fkind = 1; s_flen = 0;
        } else if (b == CTX_SOF) {
            s_fkind = 2; s_flen = 0;
        } else if (b <= CC_STATE_MAX) {
            s_state = (cc_state_t)b;              /* 单字节状态码(含 ASK=7) */
        }
    } else {
        (void)UART1_D;                            /* 异常/溢出：丢弃以清标志 */
    }
}

void comm_init(void)
{
    UART1_Init();                  /* 时钟+引脚+波特率+ TE|RE|RIE */
    s_state   = CC_SLEEP;
    s_last_rx = tb_millis();
    NVIC_ISER |= (1u << 13);       /* 使能 UART1 中断 (IRQ13) */
}

cc_state_t comm_state(void)
{
    if ((uint32_t)(tb_millis() - s_last_rx) > LINK_TIMEOUT_MS) {
        return CC_SLEEP;           /* 串口静默 → 断连 */
    }
    return s_state;
}

void comm_send_accept(void) { UART1_PutChar((unsigned char)ACCEPT_BYTE); }
void comm_send_deny(void)   { UART1_PutChar((unsigned char)DENY_BYTE);   }

/* 取一路帧（有新内容则拷到 dst 并返回1，否则0）。短暂关中断防撕裂。*/
static int get_frame(volatile char *src, volatile uint8_t *flag, char *dst, int cap)
{
    int got = 0;
    if (cap <= 0) return 0;
    __asm volatile ("cpsid i");
    if (*flag) {
        int i = 0;
        for (; i < cap - 1 && src[i]; i++) dst[i] = src[i];
        dst[i] = 0;
        *flag = 0;
        got = 1;
    }
    __asm volatile ("cpsie i");
    return got;
}

int comm_get_telemetry(char *dst, int cap) { return get_frame(s_tele, &s_tele_new, dst, cap); }
int comm_get_context(char *dst, int cap)   { return get_frame(s_ctx,  &s_ctx_new,  dst, cap); }

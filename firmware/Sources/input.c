/*
 * input.c  —  PORTA 按键中断（A=PTA14, B=PTA16, C=PTA17），写法对齐已验证的 3.2/main.c
 *   PCR=0x0A0102 (下降沿+GPIO)；按下=低；NVIC bit30；清标志写 PCR bit24。
 *   A=手动唤起挑战 / B=切换挑战(或挑战中拒绝/放行) / C=跑酷跳跃
 */
#include "derivative.h"
#include "input.h"
#include "timebase.h"

#define A_PIN 14
#define B_PIN 16
#define C_PIN 17
#define DEBOUNCE_MS 200u

static volatile btn_event_t s_event = BTN_NONE;
static volatile uint32_t    s_last_a = 0, s_last_b = 0, s_last_c = 0;
static volatile challenge_t s_mode = CH_SQUAT;

/* 覆盖 weak PORTA_IRQHandler */
void PORTA_IRQHandler(void)
{
    uint32_t now = tb_millis();

    if (PORTA_PCR14 & 0x01000000u) {                 /* PTA14 ISF */
        if ((GPIOA_PDIR & (1u << A_PIN)) == 0) {     /* 确认按下(低) */
            if ((uint32_t)(now - s_last_a) > DEBOUNCE_MS) {
                s_event = BTN_A;
                s_last_a = now;
            }
        }
        PORTA_PCR14 |= 0x01000000u;                  /* 写1清 ISF */
    }

    if (PORTA_PCR16 & 0x01000000u) {                 /* PTA16 ISF */
        if ((GPIOA_PDIR & (1u << B_PIN)) == 0) {
            if ((uint32_t)(now - s_last_b) > DEBOUNCE_MS) {
                s_event = BTN_B;
                s_mode = (s_mode == CH_SQUAT) ? CH_GAME : CH_SQUAT;
                s_last_b = now;
            }
        }
        PORTA_PCR16 |= 0x01000000u;
    }

    if (PORTA_PCR17 & 0x01000000u) {                 /* PTA17 ISF (C 键) */
        if ((GPIOA_PDIR & (1u << C_PIN)) == 0) {
            if ((uint32_t)(now - s_last_c) > DEBOUNCE_MS) {
                s_event = BTN_C;
                s_last_c = now;
            }
        }
        PORTA_PCR17 |= 0x01000000u;
    }
}

void input_init(void)
{
    SIM_SCGC5 |= (1u << 9);                  /* PORTA 时钟(bit9)；main 已 enable，保险 */
    GPIOA_PDDR &= ~((1u << A_PIN) | (1u << B_PIN) | (1u << C_PIN));   /* 输入 */
    NVIC_ISER |= 0x40000000u;                /* PORTA = IRQ30 */
    PORTA_PCR14 = 0x0A0102;                  /* 下降沿 + GPIO（同 3.2）*/
    PORTA_PCR16 = 0x0A0102;
    PORTA_PCR17 = 0x0A0102;
}

btn_event_t input_poll(void)
{
    btn_event_t e;
    __asm volatile ("cpsid i");              /* 短临界区，避免与 ISR 竞争 */
    e = s_event;
    s_event = BTN_NONE;
    __asm volatile ("cpsie i");
    return e;
}

challenge_t input_mode(void)
{
    return s_mode;
}

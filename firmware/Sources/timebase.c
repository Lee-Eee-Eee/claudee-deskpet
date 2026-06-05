/*
 * timebase.c  —  SysTick 1ms 时基
 * 核心时钟默认 FEI ~20.97MHz（启动代码不配 MCG）→ 1ms ≈ 20970 ticks。
 * 不要重配时钟，否则此处与 UART 波特率都要重算。
 */
#include "derivative.h"
#include "timebase.h"

static volatile uint32_t s_ms = 0;

/* 覆盖 kinetis_sysinit.c 里的 weak SysTick_Handler */
void SysTick_Handler(void)
{
    s_ms++;
    (void)SYST_CSR;   /* 读 CSR 清 COUNTFLAG（volatile 读不会被优化掉）*/
}

void tb_init(void)
{
    SYST_RVR = 20970;   /* 1ms @ ~20.97MHz 核心时钟 */
    SYST_CVR = 0;       /* 清当前值 */
    SYST_CSR = 0x07;    /* ENABLE | CLKSOURCE=core | TICKINT */
}

uint32_t tb_millis(void)
{
    return s_ms;        /* 32 位读在 M0+ 上为单指令，原子 */
}

int tb_due(uint32_t *last, uint32_t period_ms)
{
    uint32_t now = s_ms;
    if ((uint32_t)(now - *last) >= period_ms) {
        *last = now;
        return 1;
    }
    return 0;
}

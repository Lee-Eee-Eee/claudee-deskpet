/*
 * led.c  —  4 颗板载 LED 状态灯（仅红色）。PORTC 单写者 -> 与 LCD(PORTB RMW)/串口/蜂鸣器零竞争。
 *
 *  接线沿用 BlazarTest LED.c（同一块 Blazar 板）:
 *    组选(低有效, PORTC):  LED1=PTC12  LED2=PTC13  LED3=PTC7  LED4=PTC6
 *    颜色(高有效):         红=PTC9    （绿 PTB18 / 蓝 PTB19 故意不用 -> 避开 PORTB 与 LCD 抢）
 *  点亮某颗红灯 = 该组拉低 + PTC9 拉高; 多颗"同色"可同时点亮, 无需分时复用。
 *
 *  为什么只用 PORTC:
 *    LCD 控制脚 LCD_CS/RS/WR/RD 在 GPIOB_PDOR 上用读-改-写(|=/&=~), 每帧上千次。
 *    若中断里也 RMW 同一 GPIOB, 会丢更新。绿/蓝在 PORTB -> 不用。红+组选全在 PORTC,
 *    而 GPIOC_PDOR 整个工程只此中断写(蜂鸣器走 TPM、UART/ADC 走外设复用, 都不碰 GPIOC_PDOR),
 *    => 单写者, 即便 RMW 也无并发问题。已逐文件核对。
 *
 *  PIT(IRQ22) ~8kHz 中断: 32 级软件 PWM 做亮度, 刷新率 ~250Hz(无频闪); 呼吸/快闪相位每周期推进。
 *  PIT 时钟源 = 总线时钟(默认 FEI ~10.49MHz)。中断极短(只算一次亮度 + 写 GPIOC), 约 0.7% CPU。
 */
#include "derivative.h"
#include "led.h"

/* ---- PORTC 位掩码 ---- */
#define LED_RED   (1u << 9)              /* PTC9  红(高=亮)         */
#define G1        (1u << 12)             /* PTC12 LED1(低=选中)     */
#define G2        (1u << 13)             /* PTC13 LED2              */
#define G3        (1u << 7)              /* PTC7  LED3              */
#define G4        (1u << 6)              /* PTC6  LED4              */
#define G_ALL     (G1 | G2 | G3 | G4)

/* ---- PWM / 定时参数 ---- */
#define PWM_STEPS 32u                    /* 亮度级数; PWM 频率 = ISR_HZ / PWM_STEPS */
#define ISR_HZ    8000u                  /* PIT 中断频率 -> 刷新 ~250Hz */
#define BUS_HZ    10490000u              /* 默认 FEI 总线时钟 ~10.49MHz(PIT 时钟源) */

#define BR_FULL   PWM_STEPS              /* 满亮(=32 => PWM 恒亮) */
#define BR_WORK   28u                    /* 干活呼吸峰值 */
#define BR_IDLE   8u                     /* 空闲微亮峰值 */

/* 主循环写、ISR 读（成对更新用短临界区）*/
static volatile uint8_t s_mode  = LED_OFF;
static volatile uint8_t s_level = 0;     /* LED_BAR: 点亮几颗(0..4) */

/* 仅 ISR 内读写 */
static uint8_t  s_pwm    = 0;            /* PWM 子计数 0..PWM_STEPS-1 */
static uint16_t s_anim   = 0;            /* 动画相位, 每个 PWM 周期 +1 (~250Hz) */
static uint8_t  s_bright = 0;            /* 本周期亮度 0..PWM_STEPS */
static uint32_t s_groups = 0;            /* 本周期要点亮(拉低)的组掩码 */

/* 三角波 0..127..0（输入取低 8 位）*/
static uint8_t tri8(uint16_t phase)
{
    uint8_t p = (uint8_t)(phase & 0xFFu);
    return (p < 128u) ? p : (uint8_t)(255u - p);
}

/* 呼吸亮度: 三角波再平方(gamma≈2, 更顺眼), 缩放到 0..peak */
static uint8_t breathe(uint16_t phase, uint8_t peak)
{
    uint32_t t = tri8(phase);                       /* 0..127 */
    return (uint8_t)((t * t * peak) / (127u * 127u));/* 0..peak */
}

/* 每个 PWM 周期重算一次: 本周期亮度 s_bright + 点亮组 s_groups */
static void recompute(void)
{
    s_anim++;
    switch (s_mode) {
    case LED_BREATHE_SLOW:
        s_bright = breathe((uint16_t)(s_anim >> 2), BR_IDLE);   /* ~8s 一息, 很淡 */
        s_groups = G_ALL;
        break;
    case LED_BREATHE:
        s_bright = breathe((uint16_t)(s_anim >> 1), BR_WORK);   /* ~4s 一息, 活泼 */
        s_groups = G_ALL;
        break;
    case LED_BLINK:
        s_bright = ((s_anim >> 5) & 1u) ? BR_FULL : 0u;         /* ~0.13s 半周期, 快闪 */
        s_groups = G_ALL;
        break;
    case LED_BAR: {
        static const uint32_t bar[5] = { 0u, G1, G1|G2, G1|G2|G3, G_ALL };
        uint8_t n = (s_level > 4u) ? 4u : s_level;
        s_groups = bar[n];
        s_bright = (uint8_t)(BR_WORK - 4u + breathe((uint16_t)(s_anim >> 1), 4)); /* 24..28 轻闪, 看得清 */
        break;
    }
    case LED_FLASH:
        s_bright = BR_FULL;
        s_groups = G_ALL;
        break;
    case LED_OFF:
    default:
        s_bright = 0u;
        s_groups = 0u;
        break;
    }
}

/* 覆盖 weak PIT_IRQHandler（IRQ22, 见 kinetis_sysinit.c 向量表）*/
void PIT_IRQHandler(void)
{
    PIT_TFLG0 = PIT_TFLG_TIF_MASK;                  /* 写 1 清通道0 中断标志 */

    if (s_pwm == 0u) {                              /* 每个 PWM 周期(~250Hz)更新一次 */
        recompute();
        /* 组选: 先全关(高), 再把要亮的拉低。仅此中断写 GPIOC -> 安全 */
        GPIOC_PDOR = (GPIOC_PDOR | G_ALL) & ~s_groups;
    }

    /* 红线软件 PWM: 周期内前 s_bright 个 tick 点亮 */
    if (s_pwm < s_bright) GPIOC_PDOR |= LED_RED;
    else                  GPIOC_PDOR &= ~LED_RED;

    if (++s_pwm >= PWM_STEPS) s_pwm = 0u;
}

void led_init(void)
{
    SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;              /* PORTC 时钟(gpio_enable_port 已开, 保险) */

    /* 5 个 LED 引脚设为 GPIO(MUX=1); 用裸值 0x100 与本工程其余风格一致 */
    PORTC_PCR6  = 0x100u;
    PORTC_PCR7  = 0x100u;
    PORTC_PCR9  = 0x100u;
    PORTC_PCR12 = 0x100u;
    PORTC_PCR13 = 0x100u;

    GPIOC_PDDR |= (G_ALL | LED_RED);                /* 方向: 输出 */
    GPIOC_PDOR |= G_ALL;                            /* 组全关(高) */
    GPIOC_PDOR &= ~LED_RED;                         /* 红灭(低)   */

    /* PIT 通道0: ~8kHz 周期中断 */
    SIM_SCGC6 |= SIM_SCGC6_PIT_MASK;
    PIT_MCR    = 0u;                                /* 使能模块(MDIS=0) */
    PIT_LDVAL0 = (BUS_HZ / ISR_HZ) - 1u;            /* ≈1310 -> ~8001Hz */
    PIT_TCTRL0 = PIT_TCTRL_TIE_MASK | PIT_TCTRL_TEN_MASK;
    NVIC_ISER |= (1u << 22);                        /* PIT = IRQ22 */
}

void led_set(led_mode_t mode, uint8_t level)
{
    __asm volatile ("cpsid i");                     /* 成对更新, 防 ISR 读到半新半旧 */
    s_mode  = (uint8_t)mode;
    s_level = level;
    __asm volatile ("cpsie i");
}

void led_from_cc(cc_state_t cc)
{
    switch (cc) {
    case CC_SLEEP:    led_set(LED_OFF, 0);          break;
    case CC_IDLE:     led_set(LED_BREATHE_SLOW, 0); break;
    case CC_NEEDPERM:
    case CC_ASK:      led_set(LED_BLINK, 0);        break;  /* 求关注: 快闪 */
    case CC_DONE:     led_set(LED_FLASH, 0);        break;
    case CC_STARTED:
    case CC_WORKING:
    case CC_SUBAGENT:
    default:          led_set(LED_BREATHE, 0);      break;  /* 干活: 活泼呼吸 */
    }
}

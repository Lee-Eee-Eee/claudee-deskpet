/*
 * sfx.c  —  TPM0_CH4 (PTC8) 蜂鸣器 + ADC 旋钮音量, 非阻塞
 *  TPM 时钟 = 系统时钟/128 ≈ 164kHz; 频率 = 164000/(MOD+1)。
 *  音量: 旋钮(电位器) PTC0 = ADC0_SE14(通道14, 同 8.2/6.1) 读 0..4095，
 *        映射到 PWM 占空比 C4V = (MOD/2) * vol/4096 —— 旋到底静音、旋满最响。
 *  sfx_update() 每循环调用：到点关音 + 每 ~80ms 读旋钮(带轻滤波)更新音量。
 */
#include "derivative.h"
#include "sfx.h"
#include "timebase.h"

static uint32_t s_off   = 0;
static uint8_t  s_on    = 0;
static uint16_t s_mod   = 0;       /* 当前音的 MOD（用于按音量重算占空比）*/
static uint16_t s_vol   = 2048;    /* 旋钮音量 0..4095（默认中）*/
static uint32_t s_vol_t = 0;       /* 下次读旋钮时刻 */

static void tpm0_init(void)
{
    SIM_SOPT2 |= SIM_SOPT2_TPMSRC(1);            /* TPM 时钟源 = MCGFLLCLK */
    SIM_SOPT2 &= ~SIM_SOPT2_PLLFLLSEL_MASK;
    SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;
    SIM_SCGC6 |= SIM_SCGC6_TPM0_MASK;
    PORTC_PCR8 = 0x0300;                         /* PTC8 = TPM0_CH4 (ALT3) */
    TPM0_CNT = 0;
    TPM0_MOD = 0;
    TPM0_SC  = (0x0008 | 0x0007);                /* 系统时钟 + 128 分频 */
    TPM0_C4SC = (0x0020 | 0x0008);               /* 边沿对齐 PWM, 高在前 */
    TPM0_C4V = 0;
}

static void adc_init(void)                       /* PTC0 = ADC0_SE14（旋钮）*/
{
    SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;
    ADC0_CFG1 = 0x14;                            /* 长采样 + 12 位 + 总线时钟（同 8.2）*/
    ADC0_CFG2 = 0x00;
    ADC0_SC2  = 0x00;                            /* 软件触发, 参考 3.3V */
    ADC0_SC3  = 0x00;                            /* 单次转换 */
}

static uint16_t adc_read14(void)
{
    ADC0_SC1A = 0x0E;                            /* 选通道14(PTC0)并触发 */
    while (!(ADC0_SC1A & ADC_SC1_COCO_MASK)) { } /* 等转换完成 */
    return (uint16_t)ADC0_RA;                    /* 0..4095 */
}

static void apply_duty(void)
{
    if (s_mod == 0) { TPM0_C4V = 0; return; }
    TPM0_C4V = (uint16_t)(((uint32_t)(s_mod / 2) * s_vol) / 4096u);
}

static void set_freq(uint16_t f)
{
    if (f == 0) { s_mod = 0; TPM0_MOD = 0; TPM0_C4V = 0; return; }
    s_mod = (uint16_t)(164000u / f) - 1;
    TPM0_MOD = s_mod;
    apply_duty();
}

void sfx_init(void)
{
    tpm0_init();
    adc_init();
    s_on = 0;
    s_vol = adc_read14();
    s_vol_t = 0;
}

void sfx_tone(uint16_t freq, uint16_t dur_ms)
{
    set_freq(freq);
    s_off = tb_millis() + dur_ms;
    s_on = 1;
}

void sfx_update(void)
{
    uint32_t now = tb_millis();
    if (s_on && (int32_t)(now - s_off) >= 0) {   /* 到点关音 */
        set_freq(0);
        s_on = 0;
    }
    if ((int32_t)(now - s_vol_t) >= 0) {          /* 周期读旋钮更新音量 */
        uint16_t v = adc_read14();
        s_vol = (uint16_t)(((uint32_t)s_vol * 3 + v) / 4);   /* 轻滤波防抖 */
        s_vol_t = now + 80;
        if (s_on) apply_duty();                   /* 音在响则即时生效 */
    }
}

void sfx_tick(void) { sfx_tone(900, 60); }
void sfx_ok(void)   { sfx_tone(1568, 220); }   /* G6, 清亮 */
void sfx_deny(void) { sfx_tone(180, 300); }
void sfx_jump(void) { sfx_tone(1200, 50); }
void sfx_hit(void)  { sfx_tone(160, 200); }

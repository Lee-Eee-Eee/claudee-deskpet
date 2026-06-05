/*
 * squat.c  —  深蹲检测
 *  思路（朝向无关、无需 sqrt）：用三轴"平方幅值" m2 = x^2+y^2+z^2 与平方阈值比较。
 *  静止时 |a|≈1g → m2≈base(≈4096^2)。一个深蹲 = 下蹲(m2 跌破 low) + 起立(m2 冲过 high)。
 *  迟滞 + 连续样本去抖 + 最短周期(>=300ms) 防误计。
 *  采样建议 ~50Hz（主循环每 ~20ms 调一次）。
 */
#include "derivative.h"
#include "squat.h"
#include "timebase.h"
#include "MMA8451Q.h"

static int      s_count;
static int      s_state;       /* 0 = TOP(站立), 1 = BOTTOM(蹲下) */
static int32_t  s_base;        /* 静止基线 m2 (counts^2) */
static uint32_t s_bottom_t;
static int      s_below, s_above;

static int16_t rd_axis(unsigned char msb_reg)
{
    unsigned char hi = MMA8451Q_ReadRegister(msb_reg);
    unsigned char lo = MMA8451Q_ReadRegister((unsigned char)(msb_reg + 1));
    return (int16_t)(((int16_t)((hi << 8) | lo)) >> 2);   /* 14-bit 有符号 */
}

static int32_t read_m2(void)
{
    int32_t x = rd_axis(0x01);   /* OUT_X_MSB */
    int32_t y = rd_axis(0x03);   /* OUT_Y_MSB */
    int32_t z = rd_axis(0x05);   /* OUT_Z_MSB */
    return x * x + y * y + z * z;
}

void squat_reset(void)
{
    int i;
    int32_t acc = 0;
    for (i = 0; i < 32; i++) {
        acc += read_m2() / 32;          /* 先除再累加, 防溢出 */
    }
    s_base = acc;
    if (s_base < 1000000) {             /* 读数异常兜底: 2g 模式 1g≈4096 counts */
        s_base = 16777216;             /* 4096^2 */
    }
    s_count = 0;
    s_state = 0;
    s_below = 0;
    s_above = 0;
    s_bottom_t = 0;
}

int squat_update(void)
{
    int32_t m2   = read_m2();
    int32_t low  = (s_base / 100) * 61;    /* ≈ (0.78g)^2 */
    int32_t high = (s_base / 100) * 169;   /* ≈ (1.30g)^2 */

    if (s_state == 0) {                    /* TOP: 等下蹲 */
        if (m2 < low) {
            if (++s_below >= 2) {
                s_state = 1;
                s_bottom_t = tb_millis();
                s_above = 0;
            }
        } else {
            s_below = 0;
        }
    } else {                               /* BOTTOM: 等起立 (且距下蹲>=300ms) */
        if (m2 > high && (uint32_t)(tb_millis() - s_bottom_t) >= 300u) {
            if (++s_above >= 2) {
                s_state = 0;
                s_below = 0;
                if (s_count < SQUAT_TARGET) s_count++;
            }
        } else if (m2 <= high) {
            s_above = 0;
        }
    }
    return s_count;
}

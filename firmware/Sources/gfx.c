/*
 * gfx.c  —  绘图助手实现
 */
#include "Blazar_TFTLCD.h"
#include "gfx.h"
#include <string.h>

/* Blazar_TFTLCD.c 里的全局前景/背景色（非 static）*/
extern unsigned int POINT_COLOR, BACK_COLOR;
/* LCD_WR_REG 在 Blazar_TFTLCD.c 有定义但头文件漏声明，这里补上 */
extern void LCD_WR_REG(unsigned int data);

void gfx_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t i, j;
    unsigned char hi = (unsigned char)(color >> 8);
    unsigned char lo = (unsigned char)(color & 0xff);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            LCD_SetCursor((unsigned int)(x + i), (unsigned int)(y + j));
            LCD_WR_REG(0x2c);            /* 写 GRAM */
            LCD_WR_DATA(hi);
            LCD_WR_DATA(lo);
        }
    }
}

void gfx_draw_indexed(uint16_t x, uint16_t y, const uint8_t *map,
                      uint16_t w, uint16_t h, uint8_t scale,
                      const uint16_t *palette)
{
    uint16_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint8_t idx = map[(uint16_t)(j * w + i)];
            if (idx == 0) continue;      /* 透明 */
            gfx_fill((uint16_t)(x + i * scale), (uint16_t)(y + j * scale),
                     scale, scale, palette[idx]);
        }
    }
}

void gfx_text(uint16_t x, uint16_t y, const char *s, uint8_t size,
              uint16_t color, uint16_t bg)
{
    uint16_t cx = x;
    uint8_t  pitch = (uint8_t)(size / 2);   /* 1206->6, 1608->8 */
    POINT_COLOR = color;
    BACK_COLOR  = bg;
    while (*s) {
        LCD_ShowChar((unsigned int)cx, (unsigned int)y,
                     (unsigned int)(unsigned char)(*s), size, 0);
        cx = (uint16_t)(cx + pitch);
        s++;
    }
}

uint16_t gfx_text_w(const char *s, uint8_t size)
{
    return (uint16_t)((size / 2) * (uint16_t)strlen(s));
}

/* --- 开窗流式快速填充（P4 提速；默认不用，需上板确认 streaming 正常后再切）---
 * 复用 LCD_SetCursor 完全相同的寻址(0x2b=X 起止, 0x2a=Y 起止, USE_HORIZONTAL=1)，
 * 只把窗口 end 设为 start+size-1，然后一次性流式写 w*h 个像素，省掉逐像素设光标，
 * 速度约提升 3-4x。原驱动里被注释掉的整屏流式版只设了 1px 窗口才失败；这里设真窗口。
 * 风险：若该面板/接线对窗口流式异常，画面会乱 -> 退回用 gfx_fill 即可。*/
static void gfx_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x1 = (uint16_t)(x + w - 1), y1 = (uint16_t)(y + h - 1);
    LCD_WR_REG(0x2b);
    LCD_WR_DATA((unsigned char)(x >> 8));  LCD_WR_DATA((unsigned char)(x & 0xff));
    LCD_WR_DATA((unsigned char)(x1 >> 8)); LCD_WR_DATA((unsigned char)(x1 & 0xff));
    LCD_WR_REG(0x2a);
    LCD_WR_DATA((unsigned char)(y >> 8));  LCD_WR_DATA((unsigned char)(y & 0xff));
    LCD_WR_DATA((unsigned char)(y1 >> 8)); LCD_WR_DATA((unsigned char)(y1 & 0xff));
    LCD_WR_REG(0x2c);
}

void gfx_fill_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    unsigned char hi = (unsigned char)(color >> 8), lo = (unsigned char)(color & 0xff);
    uint32_t n;
    if (!w || !h || x >= 320 || y >= 240) return;   /* 越界(含负坐标 wrap 成的大值)直接跳过 */
    if (x + w > 320) w = (uint16_t)(320 - x);        /* 右/下裁剪到屏内 */
    if (y + h > 240) h = (uint16_t)(240 - y);
    n = (uint32_t)w * (uint32_t)h;
    gfx_set_window(x, y, w, h);
    while (n--) { LCD_WR_DATA(hi); LCD_WR_DATA(lo); }
}

/* 开窗流式不透明位图：整块 W=w*scale, H=h*scale 一次性流式写；
 * 下标 0 -> bg, 其余 -> palette[idx]。整块覆盖 => 天然无残影。
 * 右/下裁剪到屏内(左/上的负坐标须由调用方避免，因参数是 uint16_t)。
 *
 * ★方向(关键)：本面板 MADCTL=0x88(MV=0)，LCD_SetCursor 把 X 写 0x2b(PASET)、Y 写 0x2a(CASET)，
 *   开窗后 GRAM 在窗口内沿 **CASET(=Y) 轴先自增**(Y 为快轴, X 为慢轴)。
 *   所以必须按【列优先】喂像素：外层走设备 X(慢轴)、内层走设备 Y(快轴)。
 *   早期版本按行优先(X 内层)流式 -> 与硬件自增顺序相反 -> 精灵被转置/镜像(实测图标方向反)。
 * 用乘法 sj*w 取源行(M0+ 有单周期 MULS, 只避免除法即可)。*/
void gfx_blit_fast(uint16_t x, uint16_t y, const uint8_t *map,
                   uint16_t w, uint16_t h, uint8_t scale,
                   const uint16_t *palette, uint16_t bg)
{
    uint16_t W = (uint16_t)(w * scale), H = (uint16_t)(h * scale);
    uint16_t Wv, Hv, i, j, ic = 0, si = 0;
    if (!w || !h || !scale || x >= 320 || y >= 240) return;
    Wv = (uint16_t)((x + W > 320) ? (320 - x) : W);  /* 设备 X 跨度(右裁剪) */
    Hv = (uint16_t)((y + H > 240) ? (240 - y) : H);  /* 设备 Y 跨度(下裁剪) */
    gfx_set_window(x, y, Wv, Hv);
    for (i = 0; i < Wv; i++) {                        /* 外层: 设备 X(慢轴 0x2b) */
        const uint8_t *col = map + si;                /* 源第 si 列, 行距 = w */
        uint16_t jc = 0, sj = 0;
        for (j = 0; j < Hv; j++) {                    /* 内层: 设备 Y(快轴 0x2a) */
            uint8_t  idx = col[(uint16_t)(sj * w)];
            uint16_t c   = idx ? palette[idx] : bg;
            LCD_WR_DATA((unsigned char)(c >> 8));
            LCD_WR_DATA((unsigned char)(c & 0xff));
            if (++jc == scale) { jc = 0; sj++; }
        }
        if (++ic == scale) { ic = 0; si++; }
    }
}

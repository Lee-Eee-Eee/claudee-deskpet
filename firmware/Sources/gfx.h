/*
 * gfx.h  —  Claudee 绘图助手
 */
#ifndef GFX_H_
#define GFX_H_

#include <stdint.h>

/* 填充矩形 (x,y) 起、宽 w 高 h，单色 */
void gfx_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* 画索引位图：map[j*w+i] 为调色板下标；下标 0 视为透明(跳过)；
 * 每个逻辑像素放大为 scale×scale 的实心块。*/
void gfx_draw_indexed(uint16_t x, uint16_t y, const uint8_t *map,
                      uint16_t w, uint16_t h, uint8_t scale,
                      const uint16_t *palette);

/* 画 ASCII 文本（用驱动 LCD_ShowChar，x 可 >255；size=12 或 16，间距 size/2）。
 * mode=0 不透明：用 bg 填字符底色。*/
void gfx_text(uint16_t x, uint16_t y, const char *s, uint8_t size,
              uint16_t color, uint16_t bg);

/* 文本像素宽度（size/2 * 字符数），便于做居中/右对齐 */
uint16_t gfx_text_w(const char *s, uint8_t size);

/* 开窗流式快速填充（约 3-4x gfx_fill）。默认不用。*/
void gfx_fill_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* 开窗流式画索引位图（不透明：下标 0 用 bg 画，整块一次性流式写）。*/
void gfx_blit_fast(uint16_t x, uint16_t y, const uint8_t *map,
                   uint16_t w, uint16_t h, uint8_t scale,
                   const uint16_t *palette, uint16_t bg);

#endif /* GFX_H_ */

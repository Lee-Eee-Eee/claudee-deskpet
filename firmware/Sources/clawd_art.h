/*
 * clawd_art.h  —  Clawd（Claude Code 像素螃蟹吉祥物）形体
 *  眼睛、表情(眨/瞪/笑/抓狂)、钳子挥动、Claude ✶、配件 由 display.c 在形体上叠加绘制，
 *  一套形体复用到全部状态。整体由 gfx_draw_indexed/gfx_blit_fast 按 scale 放大。
 */
#ifndef CLAWD_ART_H_
#define CLAWD_ART_H_

#include <stdint.h>

#define CLAWD_W 20
#define CLAWD_H 12

/* 眼睛逻辑坐标/大小*/
#define CLAWD_EYE_L_X 6
#define CLAWD_EYE_R_X 12
#define CLAWD_EYE_Y   2
#define CLAWD_EYE_W   2
#define CLAWD_EYE_H   3

/* 两侧钳*/
#define CLAWD_HAND_L_X 0
#define CLAWD_HAND_R_X 18
#define CLAWD_HAND_Y   5
#define CLAWD_HAND_W   2
#define CLAWD_HAND_H   2

/*
 * 形体：1=橙身, 0=透明。
 * r0 平头顶(仅切 1px 圆角)；r1-4 上身(眼区)；r5-6 两侧手外凸；r7-9 下身平底；r10-11 四条短细腿。
 */
static const uint8_t clawd_body[CLAWD_W * CLAWD_H] = {
 /* r0  平头顶 */ 0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
 /* r1        */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r2  眼区  */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r3        */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r4        */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r5  手凸  */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 /* r6  手凸  */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 /* r7        */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r8        */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r9  平底  */ 0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
 /* r10 腿    */ 0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,
 /* r11 腿    */ 0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,
};

#endif /* CLAWD_ART_H_ */

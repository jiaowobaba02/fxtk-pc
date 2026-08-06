#ifndef ASCII_GRAY16_H
#define ASCII_GRAY16_H
#include <stdint.h>

/* 16px Arial 灰度字形: w/h 位图尺寸, xoff/yoff 偏移, adv 前进量 */
#ifndef GRAY_GLYPH_T_DEFINED
#define GRAY_GLYPH_T_DEFINED
typedef struct {
    uint8_t w, h;
    int8_t xoff;
    int8_t yoff;
    uint8_t adv;
    const uint8_t *px;
} gray_glyph_t;
#endif
#define ascii_gray16_ascent 15
extern const gray_glyph_t ascii_gray16[95];
#endif

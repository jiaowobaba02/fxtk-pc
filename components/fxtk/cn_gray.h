#ifndef CN_GRAY_H
#define CN_GRAY_H
#include <stdint.h>

/* 16px 微软雅黑灰度字形 (2bit/像素, 4 级抗锯齿, 按 bbox 裁剪) */
typedef struct {
    uint16_t code;
    uint32_t pixoff : 24;   /* 数据区像素偏移 (2bit/像素) */
    uint8_t w, h;
    int8_t xoff, yoff;      /* 相对 16x16 格左上角 */
} cn_gray_glyph_t;
extern const uint16_t cn_gray_count;
extern const cn_gray_glyph_t cn_gray[];
extern const uint8_t cn_gray_data[];
#endif

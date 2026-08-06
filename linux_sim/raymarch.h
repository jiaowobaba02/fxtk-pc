#ifndef RAYMARCH_H
#define RAYMARCH_H
#include <stdint.h>
/* Shadertoy 风格软件光线步进: 渲染 RGB565 到 px[w*h] */
void raymarch_render(uint16_t *px, int w, int h, float time, int max_steps);
#endif

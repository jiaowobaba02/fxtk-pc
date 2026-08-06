#ifndef GPU_RAYMARCH_H
#define GPU_RAYMARCH_H
#include <stdint.h>
/* GPU 光追通道 (独立线程, 不干扰 SDL 的 GL 上下文) */
int  gpu_raymarch_ok(void);
const char *gpu_raymarch_renderer(void);                      /* 1=GPU 可用 */
void gpu_raymarch_render(uint16_t *px, int w, int h, float time);
#endif

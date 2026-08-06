/**
 * fxtk_image_sdl.c — PC 端图片解码 (SDL2_image), 转成 RGB565 交给 fxtk 核心
 */
#include "fxtk_image.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>

fx_image_t *fx_image_load(const char *path)
{
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_Surface *s = IMG_Load(path);
    if (!s) { printf("IMG_Load failed: %s (%s)\n", path, IMG_GetError()); return NULL; }
    SDL_Surface *fmt = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(s);
    if (!fmt) return NULL;

    fx_image_t *img = fx_image_create(fmt->w, fmt->h);
    if (img) {
        uint32_t *p = (uint32_t *)fmt->pixels;
        int pitch = fmt->pitch / 4;
        for (int y = 0; y < fmt->h; y++)
            for (int x = 0; x < fmt->w; x++) {
                uint32_t c = p[y * pitch + x];
                uint8_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
                img->px[y * img->w + x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
    }
    SDL_FreeSurface(fmt);
    return img;
}

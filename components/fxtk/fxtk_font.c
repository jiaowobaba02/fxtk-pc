/**
 * fxtk_font.c — 基于 SDL_ttf 的本机字库渲染 (PC 模拟器专用)
 */
#include "fxtk.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

// 【修复】声明外部像素绘制函数
extern void fxtk_put_px(int x, int y, uint16_t c);

static TTF_Font *g_font = NULL;
static int g_font_size = 16;

void fxtk_font_init(const char *font_path, int size)
{
    if (TTF_Init() == -1) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        return;
    }
    g_font = TTF_OpenFont(font_path, size);
    g_font_size = size;
    if (!g_font) {
        printf("Font load failed (%s): %s\n", font_path, TTF_GetError());
    } else {
        printf("Font loaded: %s (size %d)\n", font_path, size);
    }
}

int fx_text_width(const char *s)
{
    if (!g_font || !s) return 0;
    int w, h;
    TTF_SizeUTF8(g_font, s, &w, &h);
    return w;
}

void fx_draw_text_c(int x, int y, const char *s, fx_color_t fg, fx_color_t bg)
{
    if (!g_font || !s) return;

    uint8_t r_fg = ((fg >> 11) & 0x1F) << 3;
    uint8_t g_fg = ((fg >> 5) & 0x3F) << 2;
    uint8_t b_fg = (fg & 0x1F) << 3;
    SDL_Color c_fg = {r_fg, g_fg, b_fg, 255};

    SDL_Surface *surf = TTF_RenderUTF8_Blended(g_font, s, c_fg);
    if (!surf) return;

    SDL_Surface *fmt_surf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surf);
    if (!fmt_surf) return;

    uint8_t r_bg = ((bg >> 11) & 0x1F) << 3;
    uint8_t g_bg = ((bg >> 5) & 0x3F) << 2;
    uint8_t b_bg = (bg & 0x1F) << 3;

    uint32_t *pixels = (uint32_t *)fmt_surf->pixels;
    int w = fmt_surf->w;
    int h = fmt_surf->h;
    
    // 【核心修复】获取正确的行跨度（以 uint32_t 为单位），防止内存错位越界闪退
    int pitch = fmt_surf->pitch / 4; 

    for (int ty = 0; ty < h; ty++) {
        for (int tx = 0; tx < w; tx++) {
            // 【核心修复】使用 pitch 计算正确的内存偏移
            uint32_t p = pixels[ty * pitch + tx];
            uint8_t alpha = (p >> 24) & 0xFF;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;

            uint8_t fr = (r * alpha + r_bg * (255 - alpha)) / 255;
            uint8_t fg_val = (g * alpha + g_bg * (255 - alpha)) / 255;
            uint8_t fb = (b * alpha + b_bg * (255 - alpha)) / 255;

            fx_color_t c = ((fr >> 3) << 11) | ((fg_val >> 2) << 5) | (fb >> 3);
            fxtk_put_px(x + tx, y + ty, c);
        }
    }
    SDL_FreeSurface(fmt_surf);
}

void fx_draw_text(int x, int y, const char *s)
{
    fx_draw_text_c(x, y, s, FX_WHITE, FX_BLACK);
}
int fx_text_width_n(const char *s, int n)
{
    if (!s || n <= 0) return 0;
    char *t = (char *)malloc((size_t)n + 1);
    memcpy(t, s, (size_t)n); t[n] = 0;
    int w = fx_text_width(t);
    free(t);
    return w;
}
void fx_draw_text_c_n(int x, int y, const char *s, int n, fx_color_t fg, fx_color_t bg)
{
    if (!s || n <= 0) return;
    char *t = (char *)malloc((size_t)n + 1);
    memcpy(t, s, (size_t)n); t[n] = 0;
    fx_draw_text_c(x, y, t, fg, bg);
    free(t);
}

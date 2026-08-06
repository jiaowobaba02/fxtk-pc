/**
 * fxtk_sdl_driver.c — SDL2 驱动 (响应式铺满版)
 * 窗口尺寸 = 逻辑分辨率；缩放时重新布局，UI 铺满整个窗口，无黑边。
 */
#include "fxtk.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INITIAL_WIDTH 480
#define INITIAL_HEIGHT 272

fx_driver_t fx_sdl_driver;

static int s_width = INITIAL_WIDTH;
static int s_height = INITIAL_HEIGHT;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint16_t *fb = NULL;

static int mouse_pressed = 0;
#define FX_KEYQUEUE 64
static fx_keyev_t g_kq[FX_KEYQUEUE]; static int g_kq_n = 0;
static int sdl_key_read(fx_keyev_t *ev);
static int g_wq[16]; static int g_wq_n = 0;
static int sdl_wheel_read(int *x, int *y, int *dy);
static void sdl_clip_set(const char *s);
static const char *sdl_clip_get(void);
static int mouse_x = 0, mouse_y = 0;

static uint16_t cur_x0, cur_y0, cur_w;
static uint32_t cur_pixel_idx = 0;

static int sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Init failed: %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow("fxtk Linux Simulator",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              INITIAL_WIDTH, INITIAL_HEIGHT,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) { printf("Window failed: %s\n", SDL_GetError()); return -1; }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { printf("Renderer failed: %s\n", SDL_GetError()); return -1; }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, s_width, s_height);
    fb = malloc((size_t)s_width * s_height * sizeof(uint16_t));
    if (!fb) { printf("fb alloc failed\n"); return -1; }
    memset(fb, 0, (size_t)s_width * s_height * sizeof(uint16_t));
    return 0;
}

/* 窗口变化：同步驱动尺寸 + 重建帧缓冲 + 重新布局（UI 铺满） */
static void sdl_apply_size(int w, int h)
{
    if (w < 160) w = 160;
    if (h < 120) h = 120;
    if (w == s_width && h == s_height) return;

    s_width = w; s_height = h;
    fx_sdl_driver.width  = (uint16_t)s_width;
    fx_sdl_driver.height = (uint16_t)s_height;

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, s_width, s_height);
    free(fb);
    fb = malloc((size_t)s_width * s_height * sizeof(uint16_t));
    if (!fb) { printf("fb realloc failed\n"); return; }
    memset(fb, 0, (size_t)s_width * s_height * sizeof(uint16_t));

    fx_layout();
    fx_repaint();   /* pixel() 坐标按新尺寸自动缩放 */
    fx_repaint();
}

static void sdl_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    cur_x0 = x0; cur_y0 = y0; cur_w = (uint16_t)(x1 - x0 + 1);
    cur_pixel_idx = 0;
}

static void sdl_push_pixels(const uint16_t *px, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        int x = cur_x0 + (int)(cur_pixel_idx % cur_w);
        int y = cur_y0 + (int)(cur_pixel_idx / cur_w);
        if (x >= 0 && x < s_width && y >= 0 && y < s_height)
            fb[y * s_width + x] = px[i];
        cur_pixel_idx++;
    }
}

static void sdl_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= s_height) continue;
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= s_width) continue;
            fb[y * s_width + x] = color;
        }
    }
}

static int sdl_touch_read(int *x, int *y, int *pressed)
{
    *x = mouse_x; *y = mouse_y; *pressed = mouse_pressed;
    return 1;
}

void sdl_update_screen(void)
{
    if (!texture || !fb) return;
    SDL_UpdateTexture(texture, NULL, fb, s_width * (int)sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);   // 1:1 铺满，无黑边
    SDL_RenderPresent(renderer);
}

void sdl_handle_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            exit(0);
        } else if (e.type == SDL_TEXTINPUT) {
            if (g_kq_n < FX_KEYQUEUE) { fx_keyev_t ev = { {0}, 0, 1 };
                strncpy(ev.utf8, e.text.text, sizeof(ev.utf8) - 1);
                ev.utf8[sizeof(ev.utf8) - 1] = 0; g_kq[g_kq_n++] = ev; }
        } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            int k = 0;
            int mod = (SDL_GetModState() & KMOD_CTRL) ? 1 : 0;
            if (e.key.keysym.sym == SDLK_BACKSPACE) k = FX_KEY_BACKSPACE;
            else if (e.key.keysym.sym == SDLK_RETURN) k = FX_KEY_RETURN;
            else if (e.key.keysym.sym == SDLK_ESCAPE) k = FX_KEY_ESCAPE;
            else if (e.key.keysym.sym == SDLK_LEFT) k = FX_KEY_LEFT;
            else if (e.key.keysym.sym == SDLK_RIGHT) k = FX_KEY_RIGHT;
            else if (e.key.keysym.sym == SDLK_HOME) k = FX_KEY_HOME;
            else if (e.key.keysym.sym == SDLK_END) k = FX_KEY_END;
            if (k && g_kq_n < FX_KEYQUEUE) { fx_keyev_t ev = { {0}, k, 1, mod }; g_kq[g_kq_n++] = ev; }
            else if (mod && g_kq_n < FX_KEYQUEUE &&
                     (e.key.keysym.sym == SDLK_c || e.key.keysym.sym == SDLK_v ||
                      e.key.keysym.sym == SDLK_x || e.key.keysym.sym == SDLK_a)) {
                fx_keyev_t ev = { {0}, 0, 1, mod };
                ev.utf8[0] = (char)e.key.keysym.sym;
                g_kq[g_kq_n++] = ev;
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                e.window.event == SDL_WINDOWEVENT_RESIZED) {
                sdl_apply_size(e.window.data1, e.window.data2);
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            mouse_x = e.button.x; mouse_y = e.button.y; mouse_pressed = 1;
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            mouse_x = e.button.x; mouse_y = e.button.y; mouse_pressed = 0;
        } else if (e.type == SDL_MOUSEWHEEL) {
            mouse_x = e.wheel.mouseX; mouse_y = e.wheel.mouseY;
            if (g_wq_n < 16) g_wq[g_wq_n++] = e.wheel.y;
        } else if (e.type == SDL_MOUSEMOTION) {
            mouse_x = e.motion.x; mouse_y = e.motion.y;
        }
    }
}

fx_driver_t fx_sdl_driver = {
    .width = INITIAL_WIDTH,
    .height = INITIAL_HEIGHT,
    .init = sdl_init,
    .set_window = sdl_set_window,
    .push_pixels = sdl_push_pixels,
    .fill_rect = sdl_fill_rect,
    .touch_read = sdl_touch_read,
    .key_read = sdl_key_read,
    .clip_set = sdl_clip_set,
    .clip_get = sdl_clip_get,
    .wheel_read = sdl_wheel_read
};

int sdl_get_width(void)  { return s_width; }
int sdl_get_height(void) { return s_height; }
static int g_textinput_on = 0;
static int sdl_key_read(fx_keyev_t *ev)
{
    if (!g_textinput_on) { SDL_StartTextInput(); g_textinput_on = 1; }
    if (!g_kq_n) return 0;
    *ev = g_kq[0];
    memmove(&g_kq[0], &g_kq[1], sizeof(fx_keyev_t) * (size_t)(--g_kq_n));
    return 1;
}

static char g_clip[4096];
static void sdl_clip_set(const char *s) { SDL_SetClipboardText(s ? s : ""); }
static const char *sdl_clip_get(void)
{
    if (!SDL_HasClipboardText()) return "";
    char *t = SDL_GetClipboardText();
    if (!t) return "";
    strncpy(g_clip, t, 4095); g_clip[4095] = 0;
    SDL_free(t);
    return g_clip;
}

static int sdl_wheel_read(int *x, int *y, int *dy)
{
    *x = mouse_x; *y = mouse_y;
    if (!g_wq_n) return 0;
    *dy = g_wq[0];
    memmove(&g_wq[0], &g_wq[1], sizeof(int) * (size_t)(--g_wq_n));
    return 1;
}

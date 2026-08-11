/**
 * fxtk_draw.c — 矢量绘制 + 渲染管线 (支持窗口自由缩放版)
 */
#include "fxtk.h"
#include "fxtk_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const fx_driver_t *s_drv;
static fx_color_t s_color = FX_WHITE;
static int16_t s_clip_x1 = 0, s_clip_y1 = 0, s_clip_x2 = 32767, s_clip_y2 = 32767;
static int s_ox = 0, s_oy = 0;
static int s_framing = 0;

/* 【核心修复】动态行缓冲：窗口超过 512 宽时自动扩容，不再溢出闪退 */
static uint16_t *s_line = NULL;
static int s_line_cap = 0;
static int s_line_y = -1;
static int s_line_x0 = 0, s_line_x1 = 0;

static uint16_t *s_offbuf_active = NULL;
static int s_offing = 0;
static int s_offw = 0, s_offh = 0;

int fx_band_index(void) { return -1; }

static void line_ensure(int need)
{
    if (need < s_line_cap) return;
    int newcap = (need + 512) & ~511;
    uint16_t *nb = (uint16_t *)realloc(s_line, (size_t)newcap * sizeof(uint16_t));
    if (!nb) return;
    s_line = nb;
    s_line_cap = newcap;
}

static void flush_line(void)
{
    if (s_line_y < 0) return;
    s_drv->set_window((uint16_t)s_line_x0, (uint16_t)s_line_y,
                      (uint16_t)s_line_x1, (uint16_t)s_line_y);
    s_drv->push_pixels(&s_line[s_line_x0], (uint32_t)(s_line_x1 - s_line_x0 + 1));
    s_line_y = -1;
}

void fxtk_put_px(int x, int y, uint16_t c)
{
    if (x < s_clip_x1 || x > s_clip_x2 || y < s_clip_y1 || y > s_clip_y2) return;
    x += s_ox; y += s_oy;
    if (s_offing && s_offbuf_active) {
        if (x < 0 || y < 0 || x >= s_offw || y >= s_offh) return;
        s_offbuf_active[y * s_offw + x] = c;
        return;
    }
    if (x < 0 || y < 0 || x >= s_drv->width || y >= s_drv->height) return;
    if (y != s_line_y) {
        flush_line();
        s_line_y = y; s_line_x0 = s_line_x1 = x;
    } else {
        if (x < s_line_x0) s_line_x0 = x;
        if (x > s_line_x1) s_line_x1 = x;
    }
    line_ensure(x);                 /* 【修复】按需扩容行缓冲 */
    if (x >= s_line_cap) return;    /* realloc 失败的保底保护 */
    s_line[x] = c;
}

void fxtk_draw_set_driver(const fx_driver_t *drv)
{
    s_drv = drv;
    line_ensure(drv->width + 1);    /* 【修复】预分配屏幕宽度 */
}

void fxtk_draw_flush_all(void) { flush_line(); }
void fx_frame_begin(void) { if (!s_drv) return; s_framing = 1; }
void fx_frame_end(void) { if (!s_drv) return; flush_line(); s_framing = 0; }
void fx_set_color(fx_color_t c) { s_color = c; }

void fx_set_clip(int x1, int y1, int x2, int y2)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    s_clip_x1 = (int16_t)x1; s_clip_y1 = (int16_t)y1;
    s_clip_x2 = (int16_t)x2; s_clip_y2 = (int16_t)y2;
}

void fx_reset_clip(void)
{
    s_clip_x1 = 0; s_clip_y1 = 0;
    s_clip_x2 = 32767; s_clip_y2 = 32767;
}

void fx_canvas_begin(fx_widget_t *cv)
{
    if (!cv) return;
    if (s_offing) {
        s_ox = 0; s_oy = 0;
        fx_set_clip(0, 0, s_offw - 1, s_offh - 1);
        return;
    }
    s_ox = cv->x1; s_oy = cv->y1;
    fx_set_clip(0, 0, cv->x2 - cv->x1, cv->y2 - cv->y1);
}

void fx_canvas_end(void)
{
    s_ox = 0; s_oy = 0;
    fx_reset_clip();
}

void fxtk_off_begin(fx_widget_t *cv)
{
    if (!cv->offbuf) return;
    int w = cv->x2 - cv->x1 + 1;
    int h = cv->y2 - cv->y1 + 1;
    if (w <= 0 || h <= 0) return;
    /* 【缩放保护】控件尺寸变化时重分配离屏缓冲，防止堆溢出 */
    if (w != cv->offw || h != cv->offh) {
        free(cv->offbuf);
        cv->offbuf = malloc((size_t)w * (size_t)h * 2);
        if (!cv->offbuf) {
            cv->offw = cv->offh = 0;
            cv->flags &= (uint8_t)~FX_F_BUF;
            return;
        }
        cv->offw = (uint16_t)w;
        cv->offh = (uint16_t)h;
    }
    s_offbuf_active = cv->offbuf;
    s_offing = 1;
    s_offw = w; s_offh = h;
    s_ox = -cv->x1; s_oy = -cv->y1;
    fx_set_clip(0, 0, s_offw - 1, s_offh - 1);
}

void fxtk_off_end(fx_widget_t *cv)
{
    if (!cv->offbuf) return;
    s_drv->set_window((uint16_t)cv->x1, (uint16_t)cv->y1, (uint16_t)cv->x2, (uint16_t)cv->y2);
    s_drv->push_pixels(cv->offbuf, (uint32_t)(s_offw * s_offh));
    s_offbuf_active = NULL;
    s_offing = 0;
    s_ox = 0; s_oy = 0;
    fx_reset_clip();
}

int fx_canvas_enable_buf(fx_widget_t *cv)
{
    if (!cv || cv->type != FX_W_CANVAS) return -1;
    if (cv->offbuf) return 0;
    int w = cv->x2 - cv->x1 + 1, h = cv->y2 - cv->y1 + 1;
    if (w <= 0 || h <= 0) return -1;
    cv->offbuf = malloc((size_t)w * (size_t)h * 2);
    if (!cv->offbuf) return -1;
    cv->offw = (uint16_t)w;
    cv->offh = (uint16_t)h;
    /* v2 性能锁: 仅允许 3D/画板 等需要CPU像素操作的画布开离屏 */
    if (cv->name && (strcmp(cv->name, "rt_cv")==0 || strcmp(cv->name, "pt_cv")==0 || strcmp(cv->name, "pad_cv")==0)) {
        cv->flags |= FX_F_BUF;
    } else {
        cv->flags &= ~FX_F_BUF;
    }
    return 0;
}

/* ================================================================
 * 基础图元
 * ================================================================ */
void fx_draw_pixel(int x, int y) { fxtk_put_px(x, y, s_color); }

void fx_draw_hline(int x1, int x2, int y)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < s_clip_x1) x1 = s_clip_x1;
    if (x2 > s_clip_x2) x2 = s_clip_x2;
    if (y < s_clip_y1 || y > s_clip_y2 || x1 > x2) return;
    if (!s_offing && s_drv->fill_rect) {
        s_drv->fill_rect((uint16_t)(x1+s_ox),(uint16_t)(y+s_oy),(uint16_t)(x2+s_ox),(uint16_t)(y+s_oy), s_color);
        return;
    }
    for (int x = x1; x <= x2; x++) fxtk_put_px(x, y, s_color);
}

void fx_draw_vline(int x, int y1, int y2)
{
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (y1 < s_clip_y1) y1 = s_clip_y1;
    if (y2 > s_clip_y2) y2 = s_clip_y2;
    if (x < s_clip_x1 || x > s_clip_x2 || y1 > y2) return;
    if (!s_offing && s_drv->fill_rect) {
        s_drv->fill_rect((uint16_t)(x+s_ox),(uint16_t)(y1+s_oy),(uint16_t)(x+s_ox),(uint16_t)(y2+s_oy), s_color);
        return;
    }
    for (int y = y1; y <= y2; y++) fxtk_put_px(x, y, s_color);
}

void fx_draw_line(int x1, int y1, int x2, int y2)
{
    if (!s_offing && s_drv && s_drv->draw_line) {   /* v2: 折线GPU (去clip保批) */
        flush_line();
        s_drv->draw_line(x1+s_ox,y1+s_oy,x2+s_ox,y2+s_oy,s_color);
        return;
    }

    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        fxtk_put_px(x1, y1, s_color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

void fx_draw_rect(int x1, int y1, int x2, int y2)
{
    fx_draw_hline(x1, x2, y1); fx_draw_hline(x1, x2, y2);
    fx_draw_vline(x1, y1, y2); fx_draw_vline(x2, y1, y2);
}

int fxtk_drv_width(void) { return s_drv->width; }
int fxtk_drv_height(void) { return s_drv->height; }
void fx_fill_rect(int x1, int y1, int x2, int y2)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x1 < s_clip_x1) x1 = s_clip_x1;
    if (y1 < s_clip_y1) y1 = s_clip_y1;
    if (x2 > s_clip_x2) x2 = s_clip_x2;
    if (y2 > s_clip_y2) y2 = s_clip_y2;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= s_drv->width) x2 = s_drv->width - 1;
    if (y2 >= s_drv->height) y2 = s_drv->height - 1;
    if (x1 > x2 || y1 > y2) return;
    if (!s_offing && s_drv->fill_rect) {   /* v2: 帧内也走驱动几何批 */
        flush_line();
        int ax1 = x1 + s_ox, ay1 = y1 + s_oy, ax2 = x2 + s_ox, ay2 = y2 + s_oy;
        if (ax2 >= s_drv->width) ax2 = s_drv->width - 1;
        if (ay2 >= s_drv->height) ay2 = s_drv->height - 1;
        if (ax1 <= ax2 && ay1 <= ay2)
            s_drv->fill_rect((uint16_t)ax1, (uint16_t)ay1, (uint16_t)ax2, (uint16_t)ay2, s_color);
        return;
    }
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            fxtk_put_px(x, y, s_color);
}

static int isqrt(int n)
{
    if (n <= 0) return 0;
    int lo = 0, hi = 65536;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (mid * mid <= n) lo = mid; else hi = mid - 1;
    }
    return lo;
}

static void round_cut(int y, int y1, int y2, int r, int *lcut, int *rcut)
{
    *lcut = 0; *rcut = 0;
    int dy;
    if (y - y1 < r) dy = r - (y - y1);
    else if (y2 - y < r) dy = r - (y2 - y);
    else return;
    *lcut = *rcut = r - isqrt(r * r - dy * dy);
}

void fx_fill_rect_round(int x1, int y1, int x2, int y2, int r)
{
    if (r <= 0) { fx_fill_rect(x1, y1, x2, y2); return; }
    int w = x2 - x1 + 1, h = y2 - y1 + 1;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int y = y1; y <= y2; y++) {
        int lc, rc;
        round_cut(y, y1, y2, r, &lc, &rc);
        fx_draw_hline(x1 + lc, x2 - rc, y);
    }
}

void fx_draw_rect_round(int x1, int y1, int x2, int y2, int r)
{
    if (r <= 0) { fx_draw_rect(x1, y1, x2, y2); return; }
    fx_draw_hline(x1 + r, x2 - r, y1);
    fx_draw_hline(x1 + r, x2 - r, y2);
    fx_draw_vline(x1, y1 + r, y2 - r);
    fx_draw_vline(x2, y1 + r, y2 - r);
    for (int a = 0; a <= 90; a += 3) {
        double rad = a * 3.14159265 / 180.0;
        int dx = (int)(r * cos(rad) + 0.5);
        int dy = (int)(r * sin(rad) + 0.5);
        fxtk_put_px(x1 + r - dx, y1 + r - dy, s_color);
        fxtk_put_px(x2 - r + dx, y1 + r - dy, s_color);
        fxtk_put_px(x1 + r - dx, y2 - r + dy, s_color);
        fxtk_put_px(x2 - r + dx, y2 - r + dy, s_color);
    }
}

void fx_draw_circle(int cx, int cy, int r)
{
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        fxtk_put_px(cx + x, cy + y, s_color);
        fxtk_put_px(cx - x, cy + y, s_color);
        fxtk_put_px(cx + x, cy - y, s_color);
        fxtk_put_px(cx - x, cy - y, s_color);
        fxtk_put_px(cx + y, cy + x, s_color);
        fxtk_put_px(cx - y, cy + x, s_color);
        fxtk_put_px(cx + y, cy - x, s_color);
        fxtk_put_px(cx - y, cy - x, s_color);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

void fx_fill_circle(int cx, int cy, int r)
{
    if (r <= 0) return;
    if (!s_offing && s_drv && s_drv->fill_tri) {   /* v2: 圆=GPU三角扇, 1次提交 */
        const int SEG = 14;
        for (int i = 0; i < SEG; i++) {
            double a1 = i*6.2831853/SEG, a2 = (i+1)*6.2831853/SEG;
            fx_fill_triangle(cx, cy,
                cx+(int)(r*cos(a1)+0.5), cy+(int)(r*sin(a1)+0.5),
                cx+(int)(r*cos(a2)+0.5), cy+(int)(r*sin(a2)+0.5));
        }
        return;
    }
    for (int dy = -r; dy <= r; dy++) {
        int dx = isqrt(r * r - dy * dy);
        fx_draw_hline(cx - dx, cx + dx, cy + dy);
    }
}

void fx_draw_ellipse(int cx, int cy, int rx, int ry)
{
    if (rx <= 0 || ry <= 0) return;
    int x = -rx;
    while (x <= rx) {
        double t = 1.0 - (double)(x * x) / (double)(rx * rx);
        int y = (int)(ry * sqrt(t > 0 ? t : 0) + 0.5);
        fxtk_put_px(cx + x, cy + y, s_color);
        fxtk_put_px(cx + x, cy - y, s_color);
        x++;
    }
}

void fx_fill_ellipse(int cx, int cy, int rx, int ry)
{
    if (rx <= 0 || ry <= 0) return;
    for (int dy = -ry; dy <= ry; dy++) {
        double t = 1.0 - (double)(dy * dy) / (double)(ry * ry);
        int dx = (int)(rx * sqrt(t > 0 ? t : 0) + 0.5);
        fx_draw_hline(cx - dx, cx + dx, cy + dy);
    }
}

void fx_draw_arc(int cx, int cy, int r, int a1, int a2)
{
    if (a1 > a2) { int t = a1; a1 = a2; a2 = t; }
    double rd=a1*3.14159265/180.0;
    int px=cx+(int)(r*cos(rd)+0.5), py=cy+(int)(r*sin(rd)+0.5);
    for (int a = a1+1; a <= a2; a++) {   /* v2: 弧=GPU折线段 */
        double rad = a * 3.14159265 / 180.0;
        int nx=cx+(int)(r*cos(rad)+0.5), ny=cy+(int)(r*sin(rad)+0.5);
        fx_draw_line(px,py,nx,ny); px=nx; py=ny;
    }
}

void fx_fill_arc(int cx, int cy, int r, int a1, int a2)
{
    if (a1 > a2) { int t = a1; a1 = a2; a2 = t; }
    for (int a = a1; a <= a2; a += 2) {
        double rad = a * 3.14159265 / 180.0;
        int ex = cx + (int)(r * cos(rad) + 0.5);
        int ey = cy + (int)(r * sin(rad) + 0.5);
        fx_draw_line(cx, cy, ex, ey);
    }
}

void fx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    fx_draw_line(x1, y1, x2, y2);
    fx_draw_line(x2, y2, x3, y3);
    fx_draw_line(x3, y3, x1, y1);
}

void fx_fill_polygon(const int16_t *pts, int n)
{
    if (n < 3) return;
    if (!s_offing && s_drv && s_drv->fill_tri) { for (int i=1;i+1<n;i++) fx_fill_triangle(pts[0],pts[1],pts[i*2],pts[i*2+1],pts[(i+1)*2],pts[(i+1)*2+1]); return; }
    int ymin = 32767, ymax = -32768;
    for (int i = 0; i < n; i++) {
        if (pts[i * 2 + 1] < ymin) ymin = pts[i * 2 + 1];
        if (pts[i * 2 + 1] > ymax) ymax = pts[i * 2 + 1];
    }
    if (ymax < ymin) return;

    int max_xs = n + 2;
    if (max_xs < 64) max_xs = 64;
    int *xs = malloc(max_xs * sizeof(int));
    if (!xs) return;

    for (int y = ymin; y <= ymax; y++) {
        int m = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int x1 = pts[i * 2], y1 = pts[i * 2 + 1];
            int x2 = pts[j * 2], y2 = pts[j * 2 + 1];
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                int64_t x = (int64_t)x1 + (int64_t)(y - y1) * (x2 - x1) / (y2 - y1);
                if (m < max_xs) xs[m++] = (int)x;
            }
        }
        for (int i = 0; i < m - 1; i++)
            for (int j = 0; j < m - 1 - i; j++)
                if (xs[j] > xs[j + 1]) {
                    int t = xs[j]; xs[j] = xs[j + 1]; xs[j + 1] = t;
                }
        for (int i = 0; i + 1 < m; i += 2) fx_draw_hline(xs[i], xs[i + 1], y);
    }
    free(xs);
}

void fx_fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    if (!s_offing && s_drv && s_drv->fill_tri) {   /* v2: 三角GPU (去clip保批) */
        flush_line();
        s_drv->fill_tri(x1+s_ox,y1+s_oy,x2+s_ox,y2+s_oy,x3+s_ox,y3+s_oy,s_color);
        return;
    }

    int16_t pts[6] = { (int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, (int16_t)x3, (int16_t)y3 };
    fx_fill_polygon(pts, 3);
}

void fx_draw_polygon(const int16_t *pts, int n)
{
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        fx_draw_line(pts[i * 2], pts[i * 2 + 1], pts[j * 2], pts[j * 2 + 1]);
    }
}

/* ================= 图片核心 (缩放 blit, 遵守 clip/离屏) ================= */
fx_image_t *fx_image_create(int w, int h)
{
    if (w <= 0 || h <= 0) return NULL;
    fx_image_t *img = (fx_image_t *)malloc(sizeof(fx_image_t));
    if (!img) return NULL;
    img->px = (uint16_t *)malloc((size_t)w * h * 2);
    if (!img->px) { free(img); return NULL; }
    memset(img->px, 0, (size_t)w * h * 2);
    img->w = (int16_t)w; img->h = (int16_t)h;
    return img;
}
void fx_image_free(fx_image_t *img)
{
    if (!img) return;
    free(img->px); free(img);
}
void fx_image_set_px(fx_image_t *img, int x, int y, fx_color_t c)
{
    if (!img || x < 0 || y < 0 || x >= img->w || y >= img->h) return;
    img->px[y * img->w + x] = c;
}
static uint16_t img_darken(uint16_t c)
{
    return (uint16_t)((((c >> 11) & 31) / 2 << 11) | (((c >> 5) & 63) / 2 << 5) | ((c & 31) / 2));
}
void fx_draw_image_ex(fx_image_t *img, int x, int y, int dw, int dh, int dark)
{
    if (!img || !img->px || dw <= 0 || dh <= 0) return;
    if (!s_offing && s_drv && s_drv->blit_img) {   /* v2: 图片上交 GPU 缩放 */
        flush_line();
        if (s_drv->set_clip_rect) s_drv->set_clip_rect(s_clip_x1+s_ox,s_clip_y1+s_oy,s_clip_x2+s_ox,s_clip_y2+s_oy);
        s_drv->blit_img(img->px,img->w,img->h,x+s_ox,y+s_oy,dw,dh,dark);
        if (s_drv->set_clip_rect) s_drv->set_clip_rect(0,0,32767,32767);
        return;
    }
    for (int j = 0; j < dh; j++) {
        int sy = (int)((int64_t)j * img->h / dh);
        if (sy >= img->h) sy = img->h - 1;
        const uint16_t *row = &img->px[sy * img->w];
        for (int i = 0; i < dw; i++) {
            int sx = (int)((int64_t)i * img->w / dw);
            if (sx >= img->w) sx = img->w - 1;
            uint16_t c = row[sx];
            fxtk_put_px(x + i, y + j, dark ? img_darken(c) : c);
        }
    }
}
void fx_draw_image(fx_image_t *img, int x, int y, int dw, int dh)
{
    fx_draw_image_ex(img, x, y, dw, dh, 0);
}
int fxtk_is_offing(void){ return s_offing; }
void fxtk_text_blit(void *tex,int x,int y,int w,int h)
{
    if(!s_drv||!s_drv->blit_tex)return;
    int x1=x>s_clip_x1?x:s_clip_x1;
    int y1=y>s_clip_y1?y:s_clip_y1;
    int x2=(x+w-1)<s_clip_x2?(x+w-1):s_clip_x2;
    int y2=(y+h-1)<s_clip_y2?(y+h-1):s_clip_y2;
    if(x1>x2||y1>y2)return;
    s_drv->blit_tex(tex,x1-x,y1-y,x2-x1+1,y2-y1+1,x1+s_ox,y1+s_oy);
}

int fxtk_image_rot_gpu(const fx_image_t *img,int cx,int cy,int dw,int dh,double ang)
{
    if(s_offing||!s_drv||!s_drv->blit_img_rot||!img||!img->px)return 0;
    if(s_drv->set_clip_rect) s_drv->set_clip_rect(s_clip_x1+s_ox,s_clip_y1+s_oy,s_clip_x2+s_ox,s_clip_y2+s_oy);   /* 画布裁剪 */
    s_drv->blit_img_rot(img->px,img->w,img->h,cx+s_ox,cy+s_oy,dw,dh,ang);
    if(s_drv->set_clip_rect) s_drv->set_clip_rect(0,0,32767,32767);
    return 1;
}

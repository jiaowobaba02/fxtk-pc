/**
 * app_desktop.c — 桌面扩展演示 (整文件终版)
 * 页5 输入 / 页6 画板 / 页7 键鼠 / 页8 压测 / 页9 滚动
 */
#include "fxtk.h"
#include "fxtk_image.h"
#include "fxtk_effects.h"
#include "fxtk_desktop.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================= 页5: 输入 ================= */
/* (无额外状态) */

/* ================= 页6: 画板 ================= */
static fx_image_t *s_pbuf = NULL;
static fx_color_t s_brush = 0xF800;
static int s_px = -1, s_py = -1;

static void on_brush(fx_widget_t *w, void *ud) { s_brush = (fx_color_t)(intptr_t)ud; }
static void on_clear(fx_widget_t *w, void *ud) {
    (void)w; (void)ud;
    if (s_pbuf) for (int i = 0; i < s_pbuf->w * s_pbuf->h; i++) s_pbuf->px[i] = FX_WHITE;
    s_px = s_py = -1;
}
static void paint_px(fx_image_t *im, int x, int y) {
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            fx_image_set_px(im, x + dx, y + dy, s_brush);
}
static void paint_line(fx_image_t *im, int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int n = (dx > dy ? dx : dy) + 1;
    for (int i = 0; i < n; i++)
        paint_px(im, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
}
static void on_paint(fx_widget_t *w, void *ud) {
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    if (!s_pbuf || s_pbuf->w != cw || s_pbuf->h != ch) {
        if (s_pbuf) fx_image_free(s_pbuf);
        s_pbuf = fx_image_create(cw, ch);
        if (s_pbuf) on_clear(NULL, NULL);
    }
    if (!s_pbuf) return;
    int mx, my, mp; fx_touch_state(&mx, &my, &mp);
    if (mp && fx_pressed() == w) {
        mx -= x1; my -= y1;
        if (s_px >= 0) paint_line(s_pbuf, s_px, s_py, mx, my);
        else paint_px(s_pbuf, mx, my);
        s_px = mx; s_py = my;
    } else { s_px = s_py = -1; }
    fx_draw_image(s_pbuf, 0, 0, cw, ch);
}

/* ================= 页7: 键鼠 ================= */
static void on_keys_view(fx_widget_t *w, void *ud) {
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    fx_set_color(FX_RGB(30, 30, 30)); fx_fill_rect(0, 0, cw - 1, ch - 1);
    char buf[96];
    fx_keyev_t k = fx_last_key();
    if (k.utf8[0]) snprintf(buf, sizeof(buf), "最后按键: %s", k.utf8);
    else if (k.key == FX_KEY_BACKSPACE) snprintf(buf, sizeof(buf), "最后按键: Backspace");
    else if (k.key == FX_KEY_RETURN) snprintf(buf, sizeof(buf), "最后按键: Enter");
    else if (k.key == FX_KEY_ESCAPE) snprintf(buf, sizeof(buf), "最后按键: Esc");
    else snprintf(buf, sizeof(buf), "随便敲键盘 / 移动鼠标试试");
    fx_draw_text_c(10, 12, buf, FX_YELLOW, FX_RGB(30, 30, 30));
    int mx, my, mp; fx_touch_state(&mx, &my, &mp);
    snprintf(buf, sizeof(buf), "鼠标: %d, %d  [%s]", mx, my, mp ? "按下" : "松开");
    fx_draw_text_c(10, 44, buf, FX_GREEN, FX_RGB(30, 30, 30));
}

/* ================= 页8: 压测 ================= */
static int s_mv_clicks = 0;
static void on_mv_click(fx_widget_t *w, void *ud) {
    (void)ud;
    s_mv_clicks++;
    char buf[32];
    snprintf(buf, sizeof(buf), "被点 %d 次", s_mv_clicks);
    fx_set_title(w, buf);
}
static long s_mv_t = 0;
static void on_move(fx_widget_t *w, void *ud) {
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    fx_set_color(FX_RGB(20, 20, 26)); fx_fill_rect(0, 0, cw - 1, ch - 1);
    s_mv_t++;
    int t = (int)s_mv_t;
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "控件移动压测 t=%d (按钮还能点)", t);
    fx_draw_text_c(10, 8, hdr, FX_GREEN, FX_RGB(20, 20, 26));

    fx_widget_t *b  = fx_find("mv_btn");
    fx_widget_t *l  = fx_find("mv_lbl");
    fx_widget_t *pr = fx_find("mv_prog");
    fx_widget_t *ck = fx_find("mv_chk");
    if (b) {
        int bx = x1 + 20 + (int)((cw - 170) * (0.5f + 0.5f * sinf(t * 0.023f)));
        int by = y1 + 30 + (int)((ch - 100) * (0.5f + 0.5f * cosf(t * 0.031f)));
        fx_widget_set_rect(b, bx, by, bx + 120, by + 36);
    }
    if (l) {
        int lx = x1 + 10 + (int)((cw - 130) * (0.5f + 0.5f * cosf(t * 0.017f)));
        int ly = y1 + 24 + (int)((ch - 70) * (0.5f + 0.5f * sinf(t * 0.041f)));
        fx_widget_set_rect(l, lx, ly, lx + 100, ly + 20);
    }
    if (pr) {
        int px = x1 + 10 + (int)((cw - 180) * (0.5f + 0.5f * sinf(t * 0.013f + 2)));
        int py = y1 + ch - 64 + (int)(18 * sinf(t * 0.05f));
        fx_set_value(pr, t % 100);
        fx_widget_set_rect(pr, px, py, px + 160, py + 14);
    }
    if (ck) {
        int cx = x1 + cw - 150 + (int)(30 * sinf(t * 0.06f));
        int cy = y1 + 30 + (int)((ch - 110) * (0.5f + 0.5f * sinf(t * 0.027f + 1)));
        fx_set_value(ck, (t / 40) & 1);
        fx_widget_set_rect(ck, cx, cy, cx + 110, cy + 22);
    }
}

/* ================= 构建 ================= */
/* ---------- 页9: 画布自绘滚动 (无子控件, 零残影) ---------- */
static int s_scroll_off = 0;
static void on_scroll_view(fx_widget_t *w, void *ud) {
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    int row_h = 40, total = 60 * row_h;
    int maxs = total - ch; if (maxs < 0) maxs = 0;
    s_scroll_off -= fx_wheel_take(w) * 24;
    if (s_scroll_off < 0) s_scroll_off = 0;
    if (s_scroll_off > maxs) s_scroll_off = maxs;

    fx_set_color(FX_RGB(245, 245, 245)); fx_fill_rect(0, 0, cw - 1, ch - 1);
    int first = s_scroll_off / row_h;
    for (int i = first; i < 60; i++) {
        int y = i * row_h - s_scroll_off;
        if (y > ch) break;
        char t[40]; snprintf(t, sizeof(t), "第 %d 行内容 (滚轮翻我)", i + 1);
        if (i & 1) {
            fx_set_color(FX_RGB(33, 150, 243));
            fx_fill_rect(10, y + 4, cw - 20, y + row_h - 6);
            int tw = fx_text_width(t);
            fx_draw_text_c((cw - tw) / 2, y + 12, t, FX_WHITE, FX_RGB(33, 150, 243));
        } else {
            fx_draw_text_c(10, y + 12, t, FX_RGB(40, 40, 40), FX_RGB(245, 245, 245));
        }
    }
    int th = ch * ch / total; if (th < 20) th = 20;
    int ty = maxs > 0 ? (int)((long)s_scroll_off * (ch - th) / maxs) : 0;
    fx_set_color(FX_GRAY);  fx_fill_rect(cw - 5, 2, cw - 2, ch - 2);
    fx_set_color(FX_LGRAY); fx_fill_rect(cw - 5, 2 + ty, cw - 2, 2 + ty + th);
}

void build_desktop_pages(void)
{
    fx_parent(fx_find("tab"));

    /* ---- 页5 输入 ---- */
    fx_label_new(pixel("6,36", "240,56"), page(5), title("点击输入框, 直接打字:"), fgcolor(FX_RGB(51, 51, 51)));
    fx_textedit_new(pixel("6,60", "444,140"), name("edit1"), page(5), title("hello 你好"));
    fx_textedit_new(pixel("6,148", "444,182"), name("edit2"), page(5), title(""), maxlen(20));
    fx_label_new(pixel("6,190", "444,236"), page(5),
                 title("拖拽框选 / Ctrl+A C V X / 方向键 Home End;\n第一个框不限字数, 第二个限 20 字(带计数)。"),
                 fgcolor(FX_RGB(120, 120, 120)));

    /* ---- 页6 画板 ---- */
    fx_canvas_new(pixel("6,32", "330,236"), name("paint_cv"), page(6), anim(1), color(FX_WHITE), call(on_paint));
    fx_button_new(pixel("340,40", "400,70"), name("br_r"), page(6), title("红"), color(FX_RGB(244, 67, 54)), call(on_brush));
    fx_button_new(pixel("410,40", "444,70"), name("br_g"), page(6), title("绿"), color(FX_RGB(76, 175, 80)), call(on_brush));
    fx_button_new(pixel("340,80", "400,110"), name("br_b"), page(6), title("蓝"), color(FX_RGB(33, 150, 243)), call(on_brush));
    fx_button_new(pixel("410,80", "444,110"), name("br_k"), page(6), title("黑"), color(FX_RGB(40, 40, 40)), call(on_brush));
    fx_button_new(pixel("340,120", "444,150"), page(6), title("清空"), color(FX_RGB(150, 150, 150)), call(on_clear));
    fx_set_cb(fx_find("br_r"), on_brush, (void *)(intptr_t)FX_RGB(244, 67, 54));
    fx_set_cb(fx_find("br_g"), on_brush, (void *)(intptr_t)FX_RGB(76, 175, 80));
    fx_set_cb(fx_find("br_b"), on_brush, (void *)(intptr_t)FX_RGB(33, 150, 243));
    fx_set_cb(fx_find("br_k"), on_brush, (void *)(intptr_t)FX_RGB(40, 40, 40));

    /* ---- 页7 键鼠 ---- */
    fx_canvas_new(pixel("6,32", "444,236"), name("keys_cv"), page(7), anim(1), color(FX_RGB(30, 30, 30)), call(on_keys_view));

    /* ---- 页8 压测 ---- */
    fx_canvas_new(pixel("6,32", "444,236"), name("move_cv"), page(8), anim(1), color(FX_RGB(20, 20, 26)), call(on_move));
    fx_button_new(pixel("100,100", "220,136"), name("mv_btn"), page(8), title("飞天按钮"), color(FX_RGB(33, 150, 243)), call(on_mv_click));
    fx_label_new(pixel("100,150", "200,170"), name("mv_lbl"), page(8), title("漂移标签"), fgcolor(FX_YELLOW));
    fx_progress_new(pixel("100,180", "260,194"), name("mv_prog"), page(8), value(0));
    fx_checkbox_new(pixel("100,200", "210,222"), name("mv_chk"), page(8), title("勾选机"), fgcolor(FX_WHITE));

    /* ---- 页9 滚动 ---- */
    fx_canvas_new(pixel("6,32", "444,236"), name("scroll_cv"), page(9), anim(1), color(FX_RGB(245, 245, 245)), call(on_scroll_view));

    fx_parent(NULL);
}
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
static fx_color_t s_brush = FX_RED;
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
static void on_paint(fx_widget_t *w, void *ud)
{
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    const int PW = 220, PH = 120;   /* 固定离屏, 拉伸 blit, resize 不重建 */
    if (!s_pbuf) { s_pbuf = fx_image_create(PW, PH); if (s_pbuf) on_clear(NULL, NULL); }
    if (!s_pbuf) return;
    int mx, my, mp; fx_touch_state(&mx, &my, &mp);
    if (mp && fx_pressed() == w) {
        int px = (mx - x1) * PW / cw, py = (my - y1) * PH / ch;
        if (s_px >= 0) paint_line(s_pbuf, s_px, s_py, px, py);
        else paint_px(s_pbuf, px, py);
        s_px = px; s_py = py;
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
#define MV_DYN_MAX 4000      /* 对齐控件池余量 (FX_MAX_WIDGETS 4096) */
static int s_mv_extra = 0;   /* 动态增加的按钮数 */
static fx_widget_t *s_dyn[MV_DYN_MAX];   /* 指针缓存, 免每帧 fx_find 线性扫描 */
static void on_mv_click(fx_widget_t *w, void *ud) {
    (void)ud;
    s_mv_clicks++;
    char buf[32];
    snprintf(buf, sizeof(buf), "被点 %d 次", s_mv_clicks);
    fx_set_title(w, buf);
}
static void on_mv_extra_click(fx_widget_t *w, void *ud) {
    (void)ud;
    char b[16];
    snprintf(b, sizeof(b), "%d", s_mv_extra);   /* 按钮窄, 点击后仅显示编号 */
    fx_set_title(w, b);
}
static long s_mv_t = 0;
static void on_move(fx_widget_t *w, void *ud) {
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    fx_set_color(FX_RGB(20, 20, 26)); fx_fill_rect(0, 0, cw - 1, ch - 1);
    s_mv_t++;
    int t = (int)s_mv_t;
    fx_repaint_rect(x1, y1, x2, y2);  /* 止闪: 脏区=整画布, 新位置不被裁 */
    int total = fxtk_widget_count();
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "控件移动压测 t=%d · 总控件 %d", t, total);
    fx_draw_text_c(10, 8, hdr, FX_GREEN, FX_RGB(20, 20, 26));

    /* 按画布实际容量布局 (全屏 1080P 可容数千个), 帧率富余则每帧 +1 */
    int cols = (cw - 16) / 24; if (cols < 1) cols = 1;
    int rows = (ch - 46) / 20; if (rows < 1) rows = 1;
    int cap = cols * rows;
    if (fxtk_fps() >= 30 && s_mv_extra < cap && s_mv_extra < MV_DYN_MAX) {
        fx_parent(fx_find("move_cv"));   /* 挂到画布下, 随画布裁剪 */
        fx_widget_t *nb = fx_button_new(pixel("0,0", "0,0"), title("动态"),
                                        line(9), color(FX_RGB(156, 39, 176)), call(on_mv_extra_click));
        fx_parent(fx_find("tab"));
        if (nb) {
            int nx = 8 + (s_mv_extra % cols) * 24, ny = 30 + (s_mv_extra / cols) * 20;
            fx_widget_fix(nb, x1 + nx, y1 + ny);   /* 固定模式 + 记录基准, 防 layout 复位 */
            fx_widget_set_rect(nb, x1 + nx, y1 + ny, x1 + nx + 20, y1 + ny + 16);
            s_dyn[s_mv_extra] = nb;
            s_mv_extra++;
        }
    }
    /* 所有动态按钮像其他控件一样漂移 (固定基准 + 正弦扰动) */
    for (int i = 0; i < s_mv_extra; i++) {
        fx_widget_t *d = s_dyn[i];
        if (!d) continue;
        int bx = x1 + 8 + (i % cols) * 24 + (int)(14 * sinf(t * 0.05f + i * 0.7f));
        int by = y1 + 30 + (i / cols) * 20 + (int)(10 * cosf(t * 0.04f + i * 1.1f));
        fx_widget_set_rect(d, bx, by, bx + 20, by + 16);
    }
    fx_draw_text_c(10, 24, "FPS 富余时每帧自动 +1 按钮", FX_LGRAY, FX_RGB(20, 20, 26));

    fx_widget_t *b  = fx_find("mv_btn");
    fx_widget_t *l  = fx_find("mv_lbl");
    fx_widget_t *pr = fx_find("mv_prog");
    fx_widget_t *ck = fx_find("mv_chk");
    if (b) {
        int bx = x1 + 20 + (int)((cw - 170) * (0.5f + 0.5f * sinf(t * 0.023f)));
        int by = y1 + 30 + (int)((ch - 100) * (0.5f + 0.5f * cosf(t * 0.031f)));
        fx_set_value(b, t % 100);
        fx_widget_set_rect(b, bx, by, bx + 120, by + 36);
    }
    if (l) {
        int lx = x1 + 10 + (int)((cw - 130) * (0.5f + 0.5f * cosf(t * 0.017f)));
        int ly = y1 + 24 + (int)((ch - 70) * (0.5f + 0.5f * sinf(t * 0.041f)));
        fx_set_value(l, t % 100);
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
/* ---------- 页9: 画布自绘滚动 (限速平滑 + UI 缩放, 全屏比例正常) ---------- */
static void on_scroll_view(fx_widget_t *w, void *ud) {
    (void)ud;
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    /* 行高/字号随 UI 缩放, 保证任意窗口下行条比例与文字一致 */
    int sc = fxtk_ui_scale();
    int row_h = 40 * sc / 100; if (row_h < 16) row_h = 16;
    int fs = 18 * sc / 100; if (fs < 10) fs = 10;
    int total = 60 * row_h;
    int off = fx_scroll_update(w, total);   /* 核心滚动: rc 手感 (滚轮/插值/重绘 全在库内) */

    fx_set_color(FX_RGB(245, 245, 245));
    fx_fill_rect(0, 0, cw - 1, ch - 1);
    int first = off / row_h;
    for (int i = first; i < 60; i++) {
        int y = i * row_h - off;
        if (y > ch) break;
        char t[40]; snprintf(t, sizeof(t), "第 %d 行内容 (滚轮翻我)", i + 1);
        if (i & 1) {
            fx_set_color(FX_RGB(33, 150, 243));
            fx_fill_rect(10, y + 4, cw - 20, y + row_h - 6);
            int tw = fxtk_text_width_size(fs, t);
            fxtk_draw_text_size(fs, (cw - tw) / 2, y + 12, t, FX_WHITE, FX_RGB(33, 150, 243));
        } else {
            fxtk_draw_text_size(fs, 10, y + 12, t, FX_RGB(40, 40, 40), FX_RGB(245, 245, 245));
        }
    }
    fx_scrollbar_draw(w, off, total);   /* 库滚动条: 轨道+滑块 */
}

static void build_newcomp_page(void);   /* 前置声明 */
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
    fx_parent(fx_find("move_cv"));   /* 移动件挂画布下: 画布轮C2补画, 不闪 */
    fx_button_new(pixel("100,100", "220,136"), name("mv_btn"), page(8), title("飞天按钮"), color(FX_RGB(33, 150, 243)), call(on_mv_click));
    fx_label_new(pixel("100,150", "200,170"), name("mv_lbl"), page(8), title("漂移标签"), fgcolor(FX_YELLOW));
    fx_progress_new(pixel("100,180", "260,194"), name("mv_prog"), page(8), value(0));
    fx_checkbox_new(pixel("100,200", "210,222"), name("mv_chk"), page(8), title("勾选机"), fgcolor(FX_WHITE));
    fx_parent(fx_find("tab"));

    /* ---- 页9 滚动 (anim: 每帧重绘, 目标+插值滚动) ---- */
    fx_canvas_new(pixel("6,32", "444,236"), name("scroll_cv"), page(9), color(FX_RGB(245, 245, 245)), call(on_scroll_view));

    fx_parent(NULL);
    build_newcomp_page();   /* 页11 新组件 */
}

/* ================= 页11: 新组件展示 ================= */
static fx_widget_t *s_nc_pick;
static const char *s_nc_names[] = { "张伟","王芳","李娜","刘洋","陈静","杨帆",
                                    "赵磊","黄敏","周涛","吴婷","Alice","Bob" };
static void on_nc_pick(fx_widget_t *w, void *ud)
{
    char b[48];
    snprintf(b, sizeof(b), "选中: %s (#%d)", s_nc_names[(int)(intptr_t)ud], (int)(intptr_t)ud);
    fx_set_title(s_nc_pick, b);
}
static fx_widget_t *s_nc_dropw=0;
static void on_nc_scale(fx_widget_t *w, void *ud){ int fs=12+fx_get_value(w)*16/100; fx_widget_t *b=fx_find("nc_big"); if(b)fx_set_fontsize(b,fs); fx_widget_t *p=fx_find("nc_picked"); if(p)fx_set_fontsize(p,fs); if(s_nc_dropw)fx_set_fontsize(s_nc_dropw,fs); fx_repaint(); }
static void on_nc_drop(fx_widget_t *w, void *ud)
{
    static const int sz[3] = { 13, 18, 24 };
    int i = (int)(intptr_t)ud; if (i < 0 || i > 2) i = 1;
    fx_set_fontsize(fx_find("nc_big"), sz[i]);
}
static void build_newcomp_page(void)
{
    fx_parent(fx_find("tab"));   /* 关键: 挂回 tab, 页闸门才生效 */
    fx_label_new(pixel("8,30", "472,50"), page(10), line(15), row(1),
                 title("新组件: 列表 / 下拉 / 多字号"), fgcolor(FX_RGB(40, 40, 40)));
    fx_label_new(pixel("8,54", "230,68"), page(10), line(10),
                 title("line(10) 超小注释"), fgcolor(FX_RGB(120, 120, 120)));
    fx_label_new(pixel("250,54", "472,68"), page(10), line(12), row(2),
                 title("line(12) 右对齐"), fgcolor(FX_RGB(120, 120, 120)));
    fx_label_new(pixel("8,72", "472,100"), page(10), name("nc_big"), line(18), row(1),
                 title("下拉切换我的字号"), fgcolor(FX_RGB(33, 100, 200)));
    fx_widget_t *list = fx_list_new_p("8,106", "200,236", 10);
    for (int i = 0; i < 12; i++) fx_list_add(list, s_nc_names[i]);
    fx_list_set_cb(list, on_nc_pick);
    fx_slider_new(pixel("220,180", "360,194"), page(10), value(44), call(on_nc_scale));
    fx_label_new(pixel("220,200", "472,214"), page(10), line(10), title("缩放上限滑杆 (0.5x~3.0x)"));
    fx_widget_t *drop = fx_drop_new_p("220,106", "360,132", 10); s_nc_dropw=drop;
    fx_drop_add(drop, "字体: 小"); fx_drop_add(drop, "字体: 中"); fx_drop_add(drop, "字体: 大");
    fx_list_set_cb(drop, on_nc_drop);
    s_nc_pick = fx_label_new(pixel("220,140", "472,164"), page(10), name("nc_picked"),
                             line(14), title("选中: -"), fgcolor(FX_RGB(33, 100, 200)));
    fx_label_new(pixel("8,240", "472,266"), page(10), line(10),
                 title("列表滚轮滚动+点击选中; 下拉靠底部自动向上弹, 弹层带滚动不越界。"),
                 fgcolor(FX_RGB(120, 120, 120)));
}

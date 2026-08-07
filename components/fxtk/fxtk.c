/**
 * fxtk.c — 规范终版: 核心 + 桌面扩展 (输入框/滚动/滚轮/图片/压测)
 * 直接整文件覆盖, 不要再打补丁!
 */
#include "fxtk_internal.h"
#include "fxtk_desktop.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "fxtk";

fx_widget_t s_root;
static fx_widget_t s_pool[FX_MAX_WIDGETS];
static const fx_driver_t *s_drv;
static fx_widget_t *s_pressed = NULL;
static fx_widget_t *s_parent = NULL;
static int s_touch_prev = 0;
static int s_last_tx = -1, s_last_ty = -1;
static int s_repaint = 1;
static int s_full = 0;
static int s_autorepaint = 1;
static fx_color_t s_bg = FX_RGB(240, 240, 240);
#define FX_DIRTY_MAX 8
static int s_dirty[FX_DIRTY_MAX][4];
static int s_dirty_n = 0;
static int s_tdbg_on = 0;
static char s_tdbg_str[48] = "touch: -";

/* 桌面扩展状态 */
static fx_widget_t *s_focus = NULL;
static fx_keyev_t s_last_key;
static int s_tick = 0, s_blink = 0;
static fx_widget_t *s_scroll_drag = NULL;
static fx_widget_t *s_wheel_tgt = NULL;
static int s_wheel_acc = 0;

/* 响应式缩放 */
#define FX_DESIGN_W 480
#define FX_DESIGN_H 272
static int s_sx1000 = 1000, s_sy1000 = 1000;

static void redraw_widget_now(fx_widget_t *w);
static int te_len(fx_widget_t *w);
static int te_sel(fx_widget_t *w, int *a, int *b);
static void te_del_range2(fx_widget_t *w, int a, int b);
static void te_insert(fx_widget_t *w, const char *utf8);
static void te_backspace(fx_widget_t *w);
static void te_move(fx_widget_t *w, int key);
static void te_caret_from_xy(fx_widget_t *w, int x, int y);
static void scroll_drag_to(fx_widget_t *w, int y);

static int widget_in_active_page(fx_widget_t *w)
{
    for (fx_widget_t *c = w; c->parent && c->parent != &s_root; c = c->parent)
        if (c->parent->type == FX_W_TAB)
            return c->page == c->parent->value;
    return 1;
}

/* ================= 控件池 ================= */
fx_widget_t *fxtk_alloc(void)
{
    for (int i = 0; i < FX_MAX_WIDGETS; i++)
        if (s_pool[i].type == FX_W_NONE) {
            memset(&s_pool[i], 0, sizeof(s_pool[i]));
            return &s_pool[i];
        }
    ESP_LOGE(TAG, "widget pool full (%d)!", FX_MAX_WIDGETS);
    return NULL;
}
void fxtk_free(fx_widget_t *w) { if (!w || w == &s_root) return; w->type = FX_W_NONE; }
void fxtk_link(fx_widget_t *parent, fx_widget_t *child)
{ child->parent = parent; child->sibling = parent->child; parent->child = child; }
void fx_parent(fx_widget_t *p) { s_parent = p; }

/* ================= 属性构造器 ================= */
static int parse_xy(const char *s, int16_t *a, int16_t *b)
{ int x, y; if (sscanf(s, "%d,%d", &x, &y) != 2) return 0; *a = (int16_t)x; *b = (int16_t)y; return 1; }
static int parse_pct(const char *s, int16_t *a, int16_t *b)
{ float x, y; if (sscanf(s, "%f,%f", &x, &y) != 2) return 0; *a = (int)(x*1000); *b = (int)(y*1000); return 1; }
fx_attr_t pixel(const char *a, const char *b)
{ fx_attr_t at = { FX_A_PIXEL, { {0} } }; parse_xy(a,&at.v.rect.x1,&at.v.rect.y1); parse_xy(b,&at.v.rect.x2,&at.v.rect.y2); return at; }
fx_attr_t percent(const char *a, const char *b)
{ fx_attr_t at = { FX_A_PERCENT, { {0} } }; parse_pct(a,&at.v.pct.p1,&at.v.pct.p2); parse_pct(b,&at.v.pct.p3,&at.v.pct.p4); return at; }
fx_attr_t grid1(const char *g) { fx_attr_t a = { FX_A_GRID, { {0} } }; a.v.grid.name = g; return a; }
fx_attr_t grid5(const char *g, int r1, int c1, int r2, int c2)
{ fx_attr_t a = { FX_A_GRID, { {0} } }; a.v.grid.name=g; a.v.grid.r1=(int16_t)r1; a.v.grid.c1=(int16_t)c1; a.v.grid.r2=(int16_t)r2; a.v.grid.c2=(int16_t)c2; return a; }
fx_attr_t title(const char *s)  { fx_attr_t a={FX_A_TITLE,{ {0} }}; a.v.str.s=s; return a; }
fx_attr_t text(const char *s)   { fx_attr_t a={FX_A_TITLE,{ {0} }}; a.v.str.s=s; return a; }
fx_attr_t name(const char *s)   { fx_attr_t a={FX_A_NAME,{ {0} }}; a.v.str.s=s; return a; }
fx_attr_t call(fx_cb_t cb)      { fx_attr_t a={FX_A_CALL,{ {0} }}; a.v.cb.cb=cb; return a; }
fx_attr_t line(int n)           { fx_attr_t a={FX_A_LINE,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t row(int n)            { fx_attr_t a={FX_A_ROW,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t color(fx_color_t c)   { fx_attr_t a={FX_A_COLOR,{ {0} }}; a.v.color.c=c; return a; }
fx_attr_t fgcolor(fx_color_t c) { fx_attr_t a={FX_A_FGCOLOR,{ {0} }}; a.v.color.c=c; return a; }
fx_attr_t border(int n)         { fx_attr_t a={FX_A_BORDER,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t radius(int n)         { fx_attr_t a={FX_A_RADIUS,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t value(int n)          { fx_attr_t a={FX_A_VALUE,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t page(int n)           { fx_attr_t a={FX_A_PAGE,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t anim(int n)           { fx_attr_t a={FX_A_ANIM,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t fx_wptr(fx_widget_t *w){ fx_attr_t a={FX_A_WIDGET,{ {0} }}; a.v.w.w=w; return a; }
fx_attr_t maxlen(int n)         { fx_attr_t a={FX_A_MAXLEN,{ {0} }}; a.v.iv.v=(int16_t)n; return a; }
fx_attr_t image(fx_image_t *img){ fx_attr_t a={FX_A_IMAGE,{ {0} }}; a.v.w.w=(fx_widget_t*)img; return a; }

/* ================= 控件创建 ================= */
fx_widget_t *fx_widget_new_impl(int type, fx_attr_t attrs[])
{
    fx_widget_t *w = fxtk_alloc();
    if (!w) return NULL;
    w->type = (uint8_t)type; w->flags = FX_F_VISIBLE; w->pos_mode = FX_POS_PIXEL;
    w->border = 1; w->radius = 4; w->fg = FX_WHITE;
    switch (type) {
    case FX_W_LABEL: case FX_W_CANVAS: case FX_W_CHECKBOX: case FX_W_IMAGE: w->bg = FX_BLACK; break;
    case FX_W_GRID: case FX_W_PANEL: case FX_W_TAB: w->bg = 0x0841; break;
    case FX_W_SLIDER: w->bg = 0x5D7C; w->fg = 0x8430; break;
    case FX_W_PROGRESS: w->bg = 0x07E0; w->fg = 0x8430; break;
    default: w->bg = 0x5D7C; break;
    }
    for (int i = 0; attrs[i].tag != FX_A_NONE; i++) {
        switch (attrs[i].tag) {
        case FX_A_PIXEL: w->pos_mode=FX_POS_PIXEL; w->ox1=attrs[i].v.rect.x1; w->oy1=attrs[i].v.rect.y1; w->ox2=attrs[i].v.rect.x2; w->oy2=attrs[i].v.rect.y2; break;
        case FX_A_PERCENT: w->pos_mode=FX_POS_PERCENT; w->px1=attrs[i].v.pct.p1; w->py1=attrs[i].v.pct.p2; w->px2=attrs[i].v.pct.p3; w->py2=attrs[i].v.pct.p4; break;
        case FX_A_GRID: w->pos_mode=FX_POS_GRID; w->grid_ref=fx_find(attrs[i].v.grid.name); w->gr1=attrs[i].v.grid.r1; w->gc1=attrs[i].v.grid.c1; w->gr2=attrs[i].v.grid.r2; w->gc2=attrs[i].v.grid.c2; break;
        case FX_A_TITLE: strncpy(w->title, attrs[i].v.str.s?attrs[i].v.str.s:"", sizeof(w->title)-1); w->title[sizeof(w->title)-1]=0; break;
        case FX_A_NAME: strncpy(w->name, attrs[i].v.str.s?attrs[i].v.str.s:"", sizeof(w->name)-1); w->name[sizeof(w->name)-1]=0; break;
        case FX_A_CALL: w->cb=attrs[i].v.cb.cb; break;
        case FX_A_LINE: w->lines=attrs[i].v.iv.v; break;
        case FX_A_ROW: w->rows=attrs[i].v.iv.v; break;
        case FX_A_COLOR: w->bg=attrs[i].v.color.c; break;
        case FX_A_FGCOLOR: w->fg=attrs[i].v.color.c; break;
        case FX_A_BORDER: w->border=(uint8_t)attrs[i].v.iv.v; break;
        case FX_A_RADIUS: w->radius=(uint8_t)attrs[i].v.iv.v; break;
        case FX_A_VALUE: w->value=attrs[i].v.iv.v; break;
        case FX_A_PAGE: w->page=attrs[i].v.iv.v; break;
        case FX_A_IMAGE: w->img=(fx_image_t*)attrs[i].v.w.w; break;
        case FX_A_MAXLEN: w->text_max=attrs[i].v.iv.v; break;
        case FX_A_ANIM: if (attrs[i].v.iv.v) w->flags|=FX_F_ANIM; break;
        default: break;
        }
    }
    fxtk_link(w->grid_ref ? w->grid_ref : (s_parent ? s_parent : &s_root), w);
    if (type == FX_W_TEXTEDIT) {
        w->text_cap = 128; w->text_buf = (char*)malloc(128);
        if (w->text_buf) { strncpy(w->text_buf, w->title, 127); w->text_buf[127]=0; }
        w->caret = w->anchor = (int)strlen(w->text_buf ? w->text_buf : "");
    }
    if (type == FX_W_TAB) { int n=1; for (const char *p=w->title; *p; p++) if (*p==',') n++; w->lines=(int16_t)n; }
    fx_layout(); fx_repaint();
    return w;
}

fx_widget_t *fx_find(const char *n)
{
    if (!n) return NULL;
    for (int i = 0; i < FX_MAX_WIDGETS; i++)
        if (s_pool[i].type != FX_W_NONE && s_pool[i].name[0] && strcmp(s_pool[i].name, n)==0) return &s_pool[i];
    return NULL;
}
static void unlink_free(fx_widget_t *w)
{
    if (!w || w == &s_root) return;
    if (w->parent) { fx_widget_t **pp=&w->parent->child; while (*pp && *pp!=w) pp=&(*pp)->sibling; if (*pp) *pp=w->sibling; }
    while (w->child) unlink_free(w->child);
    if (s_pressed==w) s_pressed=NULL;
    if (s_focus==w) s_focus=NULL;
    if (s_scroll_drag==w) s_scroll_drag=NULL;
    if (w->text_buf) { free(w->text_buf); w->text_buf=NULL; }
    if (w->offbuf) { free(w->offbuf); w->offbuf=NULL; }
    fxtk_free(w);
}
void fx_delete_impl(fx_attr_t attrs[])
{
    for (int i=0; attrs[i].tag!=FX_A_NONE; i++) {
        fx_widget_t *w = attrs[i].tag==FX_A_GRID ? fx_find(attrs[i].v.grid.name) : (attrs[i].tag==FX_A_WIDGET ? attrs[i].v.w.w : NULL);
        if (w) unlink_free(w);
    }
    fx_layout(); fx_repaint();
}

/* ================= 布局(响应式) ================= */
static void layout_children(fx_widget_t *p);
void fx_layout(void)
{
    if (!s_drv) return;
    s_root.x1=0; s_root.y1=0; s_root.x2=(int16_t)(s_drv->width-1); s_root.y2=(int16_t)(s_drv->height-1);
    s_sx1000=(int)((int32_t)s_drv->width*1000/FX_DESIGN_W);
    s_sy1000=(int)((int32_t)s_drv->height*1000/FX_DESIGN_H);
    layout_children(&s_root);
}
static void layout_children(fx_widget_t *p)
{
    for (fx_widget_t *c=p->child; c; c=c->sibling) {
        if (!(c->flags & FX_F_VISIBLE)) continue;
        switch (c->pos_mode) {
        case FX_POS_PIXEL:
            c->x1=(int16_t)(p->x1+(int32_t)c->ox1*s_sx1000/1000); c->y1=(int16_t)(p->y1+(int32_t)c->oy1*s_sy1000/1000);
            c->x2=(int16_t)(p->x1+(int32_t)c->ox2*s_sx1000/1000); c->y2=(int16_t)(p->y1+(int32_t)c->oy2*s_sy1000/1000);
            break;
        case FX_POS_PERCENT: {
            int pw=p->x2-p->x1+1, ph=p->y2-p->y1+1;
            c->x1=(int16_t)(p->x1+(int32_t)pw*c->px1/1000); c->y1=(int16_t)(p->y1+(int32_t)ph*c->py1/1000);
            c->x2=(int16_t)(p->x1+(int32_t)pw*c->px2/1000-1); c->y2=(int16_t)(p->y1+(int32_t)ph*c->py2/1000-1);
            break; }
        case FX_POS_GRID: {
            fx_widget_t *g=c->grid_ref;
            if (g && g->lines>0 && g->rows>0) {
                int cw=(g->x2-g->x1+1)/g->rows, ch=(g->y2-g->y1+1)/g->lines;
                c->x1=(int16_t)(g->x1+(c->gc1-1)*cw); c->y1=(int16_t)(g->y1+(c->gr1-1)*ch);
                c->x2=(int16_t)(g->x1+c->gc2*cw-1); c->y2=(int16_t)(g->y1+c->gr2*ch-1);
            } break; }
        }
        layout_children(c);
    }
}

/* ================= 命中测试 ================= */
static fx_widget_t *hit_test(fx_widget_t *w, int x, int y)
{
    fx_widget_t *r = NULL;
    if (w->type == FX_W_SCROLL) {
        for (fx_widget_t *c=w->child; c && !r; c=c->sibling) r = hit_test(c, x, y + w->scroll_y);
        if (!r && x>=w->x1 && x<=w->x2 && y>=w->y1 && y<=w->y2) return w;
        return r;
    }
    if (w->type == FX_W_TAB) {
        if (y>=w->y1 && y<=w->y1+FX_TAB_H-1 && x>=w->x1 && x<=w->x2) return w;
        for (fx_widget_t *c=w->child; c && !r; c=c->sibling)
            if (c->page==w->value && (c->flags & FX_F_VISIBLE)) r = hit_test(c, x, y);
        return r;
    }
    for (fx_widget_t *c=w->child; c && !r; c=c->sibling)
        if (c->flags & FX_F_VISIBLE) r = hit_test(c, x, y);
    if (r) return r;
    if (!(w->flags & FX_F_VISIBLE) || w==&s_root) return NULL;
    if (w->type==FX_W_GRID || w->type==FX_W_PANEL || w->type==FX_W_SCROLL) return NULL;
    if (x>=w->x1 && x<=w->x2 && y>=w->y1 && y<=w->y2) return w;
    return NULL;
}

/* ================= 触摸事件 ================= */
void fx_touch_press(int x, int y)
{
    s_pressed = hit_test(&s_root, x, y);
    if (s_pressed && (s_pressed->type==FX_W_TEXTEDIT || s_pressed->type==FX_W_SCROLL)
        && s_pressed->content_h > (s_pressed->y2 - s_pressed->y1)
        && x >= s_pressed->x2 - 6) {
        s_scroll_drag = s_pressed; scroll_drag_to(s_pressed, y); return;
    }
    if (s_pressed && s_pressed->type == FX_W_TEXTEDIT) {
        fx_set_focus(s_pressed); te_caret_from_xy(s_pressed, x, y); s_pressed->anchor = s_pressed->caret;
    } else if (s_pressed) fx_set_focus(NULL);
    if (s_pressed) { s_pressed->flags |= FX_F_PRESSED; redraw_widget_now(s_pressed); }
}
void fx_touch_release(int x, int y)
{
    fx_widget_t *w = hit_test(&s_root, x, y);
    fx_widget_t *p = s_pressed;
    s_pressed = NULL; s_scroll_drag = NULL;
    if (p) {
        p->flags &= (uint8_t)~FX_F_PRESSED;
        redraw_widget_now(p);
        if (w == p) {
            if (p->type == FX_W_TAB) {
                int n = p->lines>0?p->lines:1, tw=(p->x2-p->x1+1)/n, pg=(x-p->x1)/tw;
                if (pg<0) pg=0; if (pg>=n) pg=n-1;
                if (pg != p->value) { p->value=(int16_t)pg; fx_repaint_rect(p->x1,p->y1,p->x2,p->y2); }
                if (s_focus && !widget_in_active_page(s_focus)) s_focus = NULL;
            } else {
                if (p->type == FX_W_CHECKBOX) p->value = !p->value;
                if (p->cb && p->type != FX_W_CANVAS) p->cb(p, p->ud);
            }
        }
    }
}
void fx_touch_move(int x, int y)
{
    if (s_scroll_drag) { scroll_drag_to(s_scroll_drag, y); return; }
    if (s_pressed && s_pressed->type == FX_W_TEXTEDIT) { te_caret_from_xy(s_pressed, x, y); redraw_widget_now(s_pressed); return; }
    if (s_pressed && s_pressed->type == FX_W_SLIDER) {
        int w = s_pressed->x2 - s_pressed->x1 + 1;
        int v = w>0 ? (x-s_pressed->x1)*100/w : 0;
        if (v<0)v=0; if (v>100)v=100;
        if (v != s_pressed->value) { s_pressed->value=(int16_t)v; redraw_widget_now(s_pressed); if (s_pressed->cb) s_pressed->cb(s_pressed,s_pressed->ud); }
    }
}

static void redraw_widget_now(fx_widget_t *w)
{
    if (!w || !(w->flags & FX_F_VISIBLE)) return;
    if (!widget_in_active_page(w)) return;
    switch (w->type) {
    case FX_W_BUTTON: case FX_W_SLIDER: case FX_W_PROGRESS: case FX_W_CHECKBOX:
        fx_set_clip(w->x1,w->y1,w->x2,w->y2);
        if (w->type==FX_W_BUTTON) fxtk_draw_button(w);
        else if (w->type==FX_W_SLIDER) fxtk_draw_slider(w);
        else if (w->type==FX_W_PROGRESS) fxtk_draw_progress(w);
        else fxtk_draw_checkbox(w);
        fx_reset_clip(); return;
    case FX_W_CANVAS:
        fx_set_clip(w->x1,w->y1,w->x2,w->y2);
        if ((w->flags & FX_F_BUF) && w->cb) { fxtk_off_begin(w); fxtk_draw_canvas(w); fx_canvas_begin(w); w->cb(w,w->ud); fx_canvas_end(); fxtk_off_end(w); }
        else { fxtk_draw_canvas(w); if (w->cb) { fx_canvas_begin(w); w->cb(w,w->ud); fx_canvas_end(); } }
        fx_reset_clip(); return;
    default: fx_repaint_rect(w->x1,w->y1,w->x2,w->y2);
    }
}

/* ================= 绘制 ================= */
static void draw_widget(fx_widget_t *w, int cx1, int cy1, int cx2, int cy2)
{
    if (!(w->flags & FX_F_VISIBLE)) return;
    int x1=w->x1>cx1?w->x1:cx1, y1=w->y1>cy1?w->y1:cy1, x2=w->x2<cx2?w->x2:cx2, y2=w->y2<cy2?w->y2:cy2;
    if (x1<=x2 && y1<=y2) {
        fx_set_clip(x1,y1,x2,y2);
        switch (w->type) {
        case FX_W_TAB:
            fxtk_draw_tab(w);
            for (fx_widget_t *c=w->child; c; c=c->sibling) if (c->page==w->value) draw_widget(c,cx1,cy1,cx2,cy2);
            return;
        case FX_W_SCROLL: {
            fx_set_color(w->bg); fx_fill_rect(w->x1,w->y1,w->x2,w->y2);
            int dy = -w->scroll_y;
            for (fx_widget_t *c=w->child; c; c=c->sibling) {
                c->y1+=(int16_t)dy; c->y2+=(int16_t)dy;
                draw_widget(c,cx1,cy1+dy,cx2,cy2+dy);
                c->y1-=(int16_t)dy; c->y2-=(int16_t)dy;   /* 必须还原 */
            }
            int vis2=y2-y1+1, tot2=w->content_h;
            if (tot2>vis2) {
                int th=vis2*vis2/tot2; if (th<20) th=20;
                int ty=y1+(int)((long)w->scroll_y*(vis2-th)/(tot2-vis2));
                fx_set_color(FX_GRAY);  fx_fill_rect(x2-4,y1+2,x2-1,y2-2);
                fx_set_color(FX_LGRAY); fx_fill_rect(x2-4,ty,x2-1,ty+th);
            }
            return; }
        case FX_W_BUTTON: fxtk_draw_button(w); break;
        case FX_W_LABEL: fxtk_draw_label(w); break;
        case FX_W_GRID: fxtk_draw_grid(w); break;
        case FX_W_PANEL: fxtk_draw_panel(w); break;
        case FX_W_SLIDER: fxtk_draw_slider(w); break;
        case FX_W_PROGRESS: fxtk_draw_progress(w); break;
        case FX_W_CHECKBOX: fxtk_draw_checkbox(w); break;
        case FX_W_TEXTEDIT: fxtk_draw_textedit(w); break;
        case FX_W_IMAGE: fxtk_draw_image(w); break;
        case FX_W_CANVAS:
            if ((w->flags & FX_F_BUF) && w->cb) { fxtk_off_begin(w); fxtk_draw_canvas(w); fx_canvas_begin(w); w->cb(w,w->ud); fx_canvas_end(); fxtk_off_end(w); }
            else { fxtk_draw_canvas(w); if (w->cb) { fx_canvas_begin(w); w->cb(w,w->ud); fx_canvas_end(); } }
            break;
        default: break;
        }
    }
    for (fx_widget_t *c=w->child; c; c=c->sibling) draw_widget(c,cx1,cy1,cx2,cy2);
}
void fxtk_draw_all(void)
{
    if (!s_drv) return;
    fx_set_color(s_bg); fx_fill_rect(0,0,s_drv->width-1,s_drv->height-1);
    draw_widget(&s_root,0,0,s_drv->width-1,s_drv->height-1);
    fx_reset_clip();
    if (s_tdbg_on) fx_draw_text_c(2,s_drv->height-20,s_tdbg_str,FX_YELLOW,FX_BLACK);
}
static void redraw_region(int x1,int y1,int x2,int y2)
{ fx_set_color(s_bg); fx_fill_rect(x1,y1,x2,y2); draw_widget(&s_root,x1,y1,x2,y2); }
static void draw_canvas_only(fx_widget_t *w)
{
    if (!(w->flags & FX_F_VISIBLE)) return;
    if (w->type==FX_W_TAB) { for (fx_widget_t *c=w->child;c;c=c->sibling) if (c->page==w->value) draw_canvas_only(c); return; }
    if (w->type==FX_W_CANVAS && w->cb && (w->flags & FX_F_ANIM)) {
        if (w->flags & FX_F_BUF) { fxtk_off_begin(w); fxtk_draw_canvas(w); fx_canvas_begin(w); w->cb(w,w->ud); fx_canvas_end(); fxtk_off_end(w); }
        else { fxtk_draw_canvas(w); fx_canvas_begin(w); w->cb(w,w->ud); fx_canvas_end(); }
    }
    for (fx_widget_t *c=w->child;c;c=c->sibling) draw_canvas_only(c);
}
void fxtk_draw_canvases(void) { if (!s_drv) return; draw_canvas_only(&s_root); fxtk_draw_flush_all(); }

/* ================= 系统 API ================= */
void fx_init(const fx_driver_t *drv)
{
    s_drv=drv; fxtk_draw_set_driver(drv);
    memset(&s_root,0,sizeof(s_root)); s_root.type=FX_W_PANEL; s_root.flags=FX_F_VISIBLE; s_root.bg=s_bg;
    memset(s_pool,0,sizeof(s_pool));
    s_pressed=NULL; s_touch_prev=0; s_repaint=1; s_autorepaint=0;
    fx_layout(); ESP_LOGI(TAG,"fxtk ready: %dx%d",drv->width,drv->height);
}
void fx_poll(void)
{
    if (!s_drv) return;
    if (s_drv->wheel_read) {
        int wx,wy,dy;
        while (s_drv->wheel_read(&wx,&wy,&dy)) {
            fx_widget_t *t = hit_test(&s_root,wx,wy);
            while (t && t->type != FX_W_SCROLL && t->type != FX_W_TEXTEDIT && t->type != FX_W_CANVAS) t = t->parent;
            if (t) {
                if (t->type == FX_W_CANVAS) {
                    if (s_wheel_tgt != t) { s_wheel_tgt = t; s_wheel_acc = 0; }
                    s_wheel_acc += dy;
                    fx_repaint_rect(t->x1, t->y1, t->x2, t->y2);
                    continue;
                }
                int vis=(t->type==FX_W_TEXTEDIT)?(t->y2-t->y1-10):(t->y2-t->y1);
                int maxs=t->content_h-vis; if (maxs<0) maxs=0;
                t->scroll_y-=(int16_t)(dy*24);
                if (t->scroll_y<0)t->scroll_y=0; if (t->scroll_y>maxs)t->scroll_y=(int16_t)maxs;
                fx_repaint_rect(t->x1,t->y1,t->x2,t->y2);
            }
        }
    }
    if (s_drv->touch_read) {
        int x=0,y=0,p=0;
        if (s_drv->touch_read(&x,&y,&p)) {
            if (p) { s_last_tx=x; s_last_ty=y; }
            if (p && !s_touch_prev) fx_touch_press(x,y);
            else if (p && s_touch_prev) fx_touch_move(x,y);
            else if (!p && s_touch_prev) fx_touch_release(s_last_tx,s_last_ty);
            s_touch_prev=p;
            if (s_tdbg_on) snprintf(s_tdbg_str,sizeof(s_tdbg_str),"T:%3d,%3d %s",x,y,p?"DOWN":"up  ");
        }
    }
    if (s_drv->key_read) {
        fx_keyev_t ev;
        while (s_drv->key_read(&ev)) {
            s_last_key=ev;
            if (s_focus && s_focus->type==FX_W_TEXTEDIT && ev.down && widget_in_active_page(s_focus)) {
                if (ev.mod && ev.utf8[0]) {
                    char c=ev.utf8[0]|32; int a,b;
                    if (c=='a') { s_focus->anchor=0; s_focus->caret=te_len(s_focus); }
                    else if (c=='c'||c=='x') {
                        if (te_sel(s_focus,&a,&b) && s_drv->clip_set) { static char cb[4096]; int n=b-a; if(n>4095)n=4095; memcpy(cb,s_focus->text_buf+a,(size_t)n); cb[n]=0; s_drv->clip_set(cb); }
                        if (c=='x' && te_sel(s_focus,&a,&b)) te_del_range2(s_focus,a,b);
                    }
                    else if (c=='v' && s_drv->clip_get) te_insert(s_focus,s_drv->clip_get());
                } else if (ev.utf8[0]) te_insert(s_focus,ev.utf8);
                else if (ev.key==FX_KEY_BACKSPACE) { int a,b; if (te_sel(s_focus,&a,&b)) te_del_range2(s_focus,a,b); else te_backspace(s_focus); }
                else if (ev.key==FX_KEY_LEFT||ev.key==FX_KEY_RIGHT) { te_move(s_focus,ev.key); s_focus->anchor=s_focus->caret; }
                else if (ev.key==FX_KEY_HOME) { s_focus->caret=0; s_focus->anchor=0; }
                else if (ev.key==FX_KEY_END) { s_focus->caret=te_len(s_focus); s_focus->anchor=s_focus->caret; }
                fx_repaint_rect(s_focus->x1,s_focus->y1,s_focus->x2,s_focus->y2);
            }
        }
    }
    s_tick++;
    int bl=(s_tick/25)&1;
    if (s_focus && s_focus->type==FX_W_TEXTEDIT && bl!=s_blink) { s_blink=bl; fx_repaint_rect(s_focus->x1,s_focus->y1,s_focus->x2,s_focus->y2); }
    if (s_full) { s_full = 0; s_repaint = 0; s_dirty_n = 0;
fx_frame_begin(); fxtk_draw_all(); fx_frame_end(); }
else if (s_repaint && s_dirty_n > 0) {
        s_repaint=0; fx_frame_begin();
        for (int i=0;i<s_dirty_n;i++) redraw_region(s_dirty[i][0],s_dirty[i][1],s_dirty[i][2],s_dirty[i][3]);
        s_dirty_n=0; fx_frame_end();
    } else if (s_repaint) { s_repaint=0; fx_frame_begin(); fxtk_draw_all(); fx_frame_end(); }
    else if (s_autorepaint) { fx_frame_begin(); fxtk_draw_all(); fx_frame_end(); }
    else fxtk_draw_canvases();
}
uint16_t fx_width(void){return s_drv?s_drv->width:0;} uint16_t fx_height(void){return s_drv?s_drv->height:0;}
void fx_set_autorepaint(int on){s_autorepaint=on;} void fx_set_touch_debug(int on){s_tdbg_on=on;}
void fx_repaint(void){s_repaint=1;s_dirty_n=0;}
void fx_repaint_rect(int x1,int y1,int x2,int y2)
{
    if (!s_drv) return;
    if (x1<0)x1=0; if (y1<0)y1=0; if (x2>=s_drv->width)x2=s_drv->width-1; if (y2>=s_drv->height)y2=s_drv->height-1;
    if (x1>x2||y1>y2) return;
    int merged=1;
    while (merged) { merged=0;
        for (int i=0;i<s_dirty_n;i++) if (x1<=s_dirty[i][2]&&s_dirty[i][0]<=x2&&y1<=s_dirty[i][3]&&s_dirty[i][1]<=y2) {
            if (x1<s_dirty[i][0])s_dirty[i][0]=x1; if (y1<s_dirty[i][1])s_dirty[i][1]=y1;
            if (x2>s_dirty[i][2])s_dirty[i][2]=x2; if (y2>s_dirty[i][3])s_dirty[i][3]=y2;
            x1=s_dirty[i][0];y1=s_dirty[i][1];x2=s_dirty[i][2];y2=s_dirty[i][3];
            s_dirty[i][0]=s_dirty[s_dirty_n-1][0];s_dirty[i][1]=s_dirty[s_dirty_n-1][1];
            s_dirty[i][2]=s_dirty[s_dirty_n-1][2];s_dirty[i][3]=s_dirty[s_dirty_n-1][3];
            s_dirty_n--; i--; merged=1;
        }
    }
    if (s_dirty_n<FX_DIRTY_MAX) { s_dirty[s_dirty_n][0]=x1;s_dirty[s_dirty_n][1]=y1;s_dirty[s_dirty_n][2]=x2;s_dirty[s_dirty_n][3]=y2;s_dirty_n++;s_repaint=1; }
    else fx_repaint();
}
void fx_set_bg(fx_color_t c){s_bg=c;s_root.bg=c;fx_repaint();}
void fx_set_title(fx_widget_t *w,const char *s){ if(!w)return; strncpy(w->title,s?s:"",sizeof(w->title)-1); w->title[sizeof(w->title)-1]=0; redraw_widget_now(w); }
void fx_set_color_w(fx_widget_t *w,fx_color_t c){ if(!w)return; w->bg=c; redraw_widget_now(w); }
void fx_set_value(fx_widget_t *w,int v){ if(!w)return; if(v<0)v=0; if(v>100)v=100; w->value=(int16_t)v; redraw_widget_now(w); }
int fx_get_value(const fx_widget_t *w){return w?w->value:0;}
void fx_set_cb(fx_widget_t *w,fx_cb_t cb,void *ud){ if(!w)return; w->cb=cb; w->ud=ud; }
void fx_set_visible(fx_widget_t *w,int vis){ if(!w)return; if(vis)w->flags|=FX_F_VISIBLE; else w->flags&=(uint8_t)~FX_F_VISIBLE; fx_layout(); fx_repaint(); }
int fx_widget_type(const fx_widget_t *w){return w?w->type:FX_W_NONE;}
const char *fx_widget_title(const fx_widget_t *w){return w?w->title:NULL;}
void fx_widget_rect(const fx_widget_t *w,int *x1,int *y1,int *x2,int *y2){ if(!w)return; if(x1)*x1=w->x1; if(y1)*y1=w->y1; if(x2)*x2=w->x2; if(y2)*y2=w->y2; }
fx_color_t fx_get_bg(void){return s_bg;}

/* ================= TEXTEDIT 内核 ================= */
static int te_len(fx_widget_t *w){return (int)strlen(w->text_buf?w->text_buf:"");}
static int te_chars_n(const char *s,int n){int c=0,i=0;while(i<n&&s[i]){if(((unsigned char)s[i]&0xC0)!=0x80)c++;i++;}return c;}
static int te_chars(fx_widget_t *w){return te_chars_n(w->text_buf?w->text_buf:"",te_len(w));}
static void te_grow(fx_widget_t *w,int need){ if(need<w->text_cap)return; int nc=need*2; char *nb=(char*)realloc(w->text_buf,(size_t)nc); if(nb){w->text_buf=nb;w->text_cap=nc;} }
static int te_prev_off(const char *s,int off){int i=off-1;while(i>0&&(((unsigned char)s[i]&0xC0)==0x80))i--;return i<0?0:i;}
static int te_next_off(const char *s,int off){int len=(int)strlen(s),i=off+1;while(i<len&&(((unsigned char)s[i]&0xC0)==0x80))i++;return i>len?len:i;}
static int te_sel(fx_widget_t *w,int *a,int *b){int x=w->caret,y=w->anchor;if(x>y){int t=x;x=y;y=t;}*a=x;*b=y;return y>x;}
static void te_del_range2(fx_widget_t *w,int a,int b){char *s=w->text_buf;if(!s||a>=b)return;int len=(int)strlen(s);memmove(s+a,s+b,(size_t)(len-b)+1);w->caret=a;w->anchor=a;}
static void te_insert(fx_widget_t *w,const char *utf8)
{
    char *s=w->text_buf; if(!s||!utf8)return;
    int a,b; if (te_sel(w,&a,&b)) te_del_range2(w,a,b);
    int ul=(int)strlen(utf8);
    if (w->text_max>0) { int free_n=w->text_max-te_chars(w); if (free_n<=0)return;
        if (te_chars_n(utf8,ul)>free_n) { int i=0,c=0; while(i<ul&&c<free_n){if(((unsigned char)utf8[i]&0xC0)!=0x80)c++;i++;} ul=i; if(ul<=0)return; } }
    int len=(int)strlen(s); te_grow(w,len+ul+1); s=w->text_buf;
    memmove(s+w->caret+ul,s+w->caret,(size_t)(len-w->caret)+1);
    memcpy(s+w->caret,utf8,(size_t)ul);
    w->caret+=ul; w->anchor=w->caret;
}
static void te_backspace(fx_widget_t *w){ if(w->caret<=0)return; int p=te_prev_off(w->text_buf,w->caret); te_del_range2(w,p,w->caret); }
static void te_move(fx_widget_t *w,int key){ if(key==FX_KEY_LEFT)w->caret=te_prev_off(w->text_buf,w->caret); else w->caret=te_next_off(w->text_buf,w->caret); }
static void te_wrap(fx_widget_t *w,const char *txt,int *st,int max,int *nl)
{
    int len=(int)strlen(txt), aw=(w->x2-w->x1+1)-12;
    *nl=1; st[0]=0; int acc=0,i=0;
    while (i<len && *nl<max) { int j=te_next_off(txt,i); int cw=fx_text_width_n(txt+i,j-i);
        if (acc+cw>aw && j>st[*nl-1]) { st[(*nl)++]=i; acc=0; continue; } acc+=cw; i=j; }
    st[*nl]=len;
}
static void te_caret_from_xy(fx_widget_t *w,int x,int y)
{
    const char *s=w->text_buf?w->text_buf:""; static int st[512]; int nl;
    te_wrap(w,s,st,511,&nl);
    int lh=22, ty0=w->y1+5, L=(y-ty0+w->scroll_y)/lh;
    if (L<0)L=0; if (L>=nl)L=nl-1;
    int rel=x-(w->x1+6), i=st[L], acc=0;
    while (i<st[L+1]) { int j=te_next_off(s,i); int cw=fx_text_width_n(s+i,j-i); if (rel<=acc+cw/2) break; acc+=cw; i=j; }
    w->caret=i;
}
static void scroll_drag_to(fx_widget_t *w,int y)
{
    int vis=(w->type==FX_W_TEXTEDIT)?(w->y2-w->y1-10):(w->y2-w->y1+1);
    int total=w->content_h; if (total<=vis) return;
    int track0=w->y1+2, track1=w->y2-2, th=vis*vis/total; if (th<20)th=20;
    int span=(track1-track0)-th; if (span<=0) return;
    int sc=(int)(((long)(y-track0-th/2)*(total-vis))/span);
    if (sc<0)sc=0; if (sc>total-vis)sc=total-vis;
    if (sc!=w->scroll_y) { w->scroll_y=(int16_t)sc; fx_repaint_rect(w->x1,w->y1,w->x2,w->y2); }
}
const char *fx_textedit_get(fx_widget_t *w){return (w&&w->text_buf)?w->text_buf:(w?w->title:"");}

/* ================= 桌面扩展 API ================= */
void fx_set_focus(fx_widget_t *w){ s_focus=w; if (w) fx_repaint_rect(w->x1,w->y1,w->x2,w->y2); }
fx_widget_t *fx_get_focus(void){return s_focus;}
int fx_focus_blink(void){return s_blink;}
fx_keyev_t fx_last_key(void){return s_last_key;}
void fx_touch_state(int *x,int *y,int *pressed){ if(x)*x=s_last_tx; if(y)*y=s_last_ty; if(pressed)*pressed=s_touch_prev; }
fx_widget_t *fx_pressed(void){return s_pressed;}
void fx_set_image(fx_widget_t *w,fx_image_t *img){ if(!w)return; w->img=img; fx_repaint_rect(w->x1,w->y1,w->x2,w->y2); }
void fx_image_set_zoom(fx_widget_t *w,int p){ if(!w||w->type!=FX_W_IMAGE)return; if(p<10)p=10; if(p>400)p=400; w->value=(int16_t)p; fx_repaint_rect(w->x1,w->y1,w->x2,w->y2); }
void fx_widget_set_rect(fx_widget_t *w,int x1,int y1,int x2,int y2)
{
    if (!w) return;
    if (x1!=w->x1||y1!=w->y1||x2!=w->x2||y2!=w->y2) {
        fx_repaint_rect(w->x1,w->y1,w->x2,w->y2);
        w->x1=(int16_t)x1;w->y1=(int16_t)y1;w->x2=(int16_t)x2;w->y2=(int16_t)y2;
        fx_repaint_rect(w->x1,w->y1,w->x2,w->y2);
    }
}
void fx_scroll_content(fx_widget_t *w,int h){ if (w) w->content_h=(int16_t)h; }
void fx_set_fgcolor(fx_widget_t *w,fx_color_t c){ if(!w)return; w->fg=c; fx_repaint_rect(w->x1,w->y1,w->x2,w->y2); }
int fx_wheel_take(fx_widget_t *w) { if (s_wheel_tgt != w) return 0; int d = s_wheel_acc; s_wheel_acc = 0; return d; }

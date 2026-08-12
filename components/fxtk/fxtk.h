/**
 * fxtk.h — FX Toolkit 公共头文件
 */
#ifndef FXTK_H
#define FXTK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t fx_color_t;

#define FX_RGB(r, g, b) \
    ((fx_color_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define FX_RED    0xF800
#define FX_GREEN  0x07E0
#define FX_BLUE   0x001F
#define FX_WHITE  0xFFFF
#define FX_BLACK  0x0000
#define FX_YELLOW 0xFFE0
#define FX_CYAN   0x07FF
#define FX_MAGENTA 0xF81F
#define FX_GRAY   0x8430
#define FX_LGRAY  0xC618

struct fx_widget;
typedef struct fx_widget fx_widget_t;
typedef void (*fx_cb_t)(fx_widget_t *w, void *ud);

typedef enum {
    FX_A_NONE = 0,
    FX_A_PIXEL,
    FX_A_PERCENT,
    FX_A_GRID,
    FX_A_TITLE,
    FX_A_NAME,
    FX_A_CALL,
    FX_A_LINE,
    FX_A_ROW,
    FX_A_COLOR,
    FX_A_FGCOLOR,
    FX_A_DENSE,
    FX_A_BORDER,
    FX_A_RADIUS,
    FX_A_VALUE,
    FX_A_WIDGET,
    FX_A_PAGE,
    FX_A_ANIM,
} fx_attr_tag_t;

typedef struct {
    fx_attr_tag_t tag;
    union {
        struct { int16_t x1, y1, x2, y2; } rect;
        struct { int16_t p1, p2, p3, p4; } pct;
        struct { const char *name; int16_t r1, c1, r2, c2; } grid;
        struct { const char *s; } str;
        struct { fx_cb_t cb; } cb;
        struct { int16_t v; } iv;
        struct { fx_color_t c; } color;
        struct { fx_widget_t *w; } w;
    } v;
} fx_attr_t;

#define FX_ATTR_END { FX_A_NONE, { {0} } }

fx_attr_t pixel(const char *a, const char *b);
fx_attr_t percent(const char *a, const char *b);
fx_attr_t grid1(const char *name);
fx_attr_t grid5(const char *name, int r1, int c1, int r2, int c2);
#define FX_GRID_SEL(_1,_2,_3,_4,_5,NAME,...) NAME
#define grid(...) FX_GRID_SEL(__VA_ARGS__, grid5, grid5, grid5, grid5, grid1)(__VA_ARGS__)
fx_attr_t title(const char *s);
fx_attr_t text(const char *s);
fx_attr_t name(const char *s);
fx_attr_t call(fx_cb_t cb);
fx_attr_t line(int n);
fx_attr_t row(int n);
fx_attr_t color(fx_color_t c);
fx_attr_t fgcolor(fx_color_t c);
fx_attr_t border(int n);
fx_attr_t radius(int n);
#define FX_POS_FIXED 3   /* 布局不重算(弹层直写坐标) */
#define FX_F_DENSE (1<<7)   /* grid 密集模式 */
#define FX_F_FIT   (1<<8)   /* 适应: 封顶+居中 */
#define FX_F_READONLY (1<<9)   /* 文本框只读 */
void fx_textedit_set_readonly(fx_widget_t *w,int ro);
void fx_set_fit(fx_widget_t *w, int on);
void fxtk_fit_rect(fx_widget_t *w,int*x1,int*y1,int*x2,int*y2);
int  fxtk_max_scale1000(void);
fx_attr_t dense(void);
fx_attr_t value(int n);
fx_attr_t page(int n);
fx_attr_t anim(int n);
fx_attr_t fx_wptr(fx_widget_t *w);

enum {
    FX_W_NONE = 0,
    FX_W_BUTTON,
    FX_W_LABEL,
    FX_W_GRID,
    FX_W_CANVAS,
    FX_W_SLIDER,
    FX_W_PROGRESS,
    FX_W_CHECKBOX,
    FX_W_PANEL,
    FX_W_TAB,
    FX_W_COUNT
};

fx_widget_t *fx_widget_new_impl(int type, fx_attr_t attrs[]);

#define fx_button_new(...)   fx_widget_new_impl(FX_W_BUTTON,   (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_label_new(...)    fx_widget_new_impl(FX_W_LABEL,    (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_grid_map(...)     fx_widget_new_impl(FX_W_GRID,     (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_canvas_new(...)   fx_widget_new_impl(FX_W_CANVAS,   (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_slider_new(...)   fx_widget_new_impl(FX_W_SLIDER,   (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_progress_new(...) fx_widget_new_impl(FX_W_PROGRESS, (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_checkbox_new(...) fx_widget_new_impl(FX_W_CHECKBOX, (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_panel_new(...)    fx_widget_new_impl(FX_W_PANEL,    (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
#define fx_tab_new(...)      fx_widget_new_impl(FX_W_TAB,      (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})

void fx_parent(fx_widget_t *p);
fx_widget_t *fx_find(const char *name);
void fx_delete_impl(fx_attr_t attrs[]);
#define fx_delete(...) fx_delete_impl((fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})

void fx_set_title(fx_widget_t *w, const char *s);
void fx_set_color_w(fx_widget_t *w, fx_color_t c);
void fx_set_value(fx_widget_t *w, int v);
int  fx_get_value(const fx_widget_t *w);
void fx_set_cb(fx_widget_t *w, fx_cb_t cb, void *ud);
void fx_set_visible(fx_widget_t *w, int vis);
int  fx_widget_type(const fx_widget_t *w);
const char *fx_widget_title(const fx_widget_t *w);
void fx_widget_set_rect(fx_widget_t *w, int x1, int y1, int x2, int y2);
void fx_widget_rect(const fx_widget_t *w, int *x1, int *y1, int *x2, int *y2);

void fx_layout(void);
void fx_set_max_scale(float f);   /* 控件缩放上限(倍), 默认2.5 */
void fxtk_apply_fit(fx_widget_t *w);
void fx_touch_press(int x, int y);
void fx_touch_release(int x, int y);
void fx_touch_move(int x, int y);

void fx_frame_begin(void);
void fx_frame_end(void);
int  fx_band_index(void);
void fx_repaint(void);
void fx_repaint_rect(int x1, int y1, int x2, int y2);

void fx_set_color(fx_color_t c);
void fx_set_clip(int x1, int y1, int x2, int y2);
void fx_reset_clip(void);
void fx_draw_pixel(int x, int y);
void fx_draw_hline(int x1, int x2, int y);
void fx_draw_vline(int x, int y1, int y2);
void fx_draw_line(int x1, int y1, int x2, int y2);
void fx_draw_rect(int x1, int y1, int x2, int y2);
void fx_fill_rect(int x1, int y1, int x2, int y2);
void fx_draw_rect_round(int x1, int y1, int x2, int y2, int r);
void fx_fill_rect_round(int x1, int y1, int x2, int y2, int r);
void fx_draw_circle(int cx, int cy, int r);
void fx_fill_circle(int cx, int cy, int r);
void fx_draw_ellipse(int cx, int cy, int rx, int ry);
void fx_fill_ellipse(int cx, int cy, int rx, int ry);
void fx_draw_arc(int cx, int cy, int r, int a1, int a2);
void fx_fill_arc(int cx, int cy, int r, int a1, int a2);
void fx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void fx_fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void fx_draw_polygon(const int16_t *pts, int n);
void fx_fill_polygon(const int16_t *pts, int n);
void fx_draw_text(int x, int y, const char *s);
void fx_draw_text_c(int x, int y, const char *s, fx_color_t fg, fx_color_t bg);
int  fx_text_width(const char *s);
void fx_canvas_begin(fx_widget_t *cv);
void fx_canvas_end(void);
int  fx_canvas_enable_buf(fx_widget_t *cv);

/* 桌面扩展: 键盘事件 */
typedef struct { char utf8[64]; int key; int down; int mod; } fx_keyev_t;
enum { FX_KEY_BACKSPACE = 8, FX_KEY_RETURN = 13, FX_KEY_ESCAPE = 27,
       FX_KEY_LEFT = 1, FX_KEY_RIGHT = 2, FX_KEY_HOME = 3, FX_KEY_END = 4,
       FX_KEY_UP = 5, FX_KEY_DOWN = 6, FX_KEY_DELETE = 127 };

typedef struct {
    uint16_t width, height;
    int  (*init)(void);
    void (*set_window)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void (*push_pixels)(const uint16_t *px, uint32_t n);
    void (*hold_begin)(void);
    void (*hold_end)(void);
    void (*fill_rect)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    int  (*touch_read)(int *x, int *y, int *pressed);
    int  (*key_read)(fx_keyev_t *ev);
    void (*clip_set)(const char *s);          /* 可选: 系统剪贴板 */
    const char *(*clip_get)(void);
    void (*set_title)(const char *s);      /* 可选: 窗口标题 */
    int  (*wheel_read)(int *x, int *y, int *dy);  /* 可选: 滚轮 */        /* 可选: 键盘事件 */
    void (*blit_img)(const uint16_t *px,int w,int h,int dx,int dy,int dw,int dh,int dark); /* GPU缩放blit */
    void (*set_clip_rect)(int x1,int y1,int x2,int y2);
    void (*fill_tri)(int x1,int y1,int x2,int y2,int x3,int y3,uint16_t c); /* GPU三角 */
    void (*draw_line)(int x1,int y1,int x2,int y2,uint16_t c); /* GPU折线 */
    void (*blit_tex)(void *tex,int sx,int sy,int sw,int sh,int dx,int dy); /* GPU文字blit(src+dst) */
    void (*blit_img_rot)(const uint16_t *px,int w,int h,int cx,int cy,int dw,int dh,double ang); /* GPU旋转blit */
} fx_driver_t;

void fx_init(const fx_driver_t *drv);
void fx_poll(void);
uint16_t fx_width(void);
uint16_t fx_height(void);
void fx_set_autorepaint(int on);
void fx_set_touch_debug(int on);
void fx_set_bg(fx_color_t c);
void fx_set_window_title(const char *s);
void fx_set_grid_lines(int on);   /* 调试: 显示网格线 */
int fxtk_grid_lines_on(void);  /* 运行时改窗口标题 */

extern fx_driver_t fx_st6201_driver;
int  fx_gt911_init(void);
int  fx_gt911_read(int *x, int *y, int *pressed);
fx_color_t fx_get_bg(void);

#ifdef __cplusplus
}
#endif

/* 核心丝滑滚动 */
int  fx_scroll_update(fx_widget_t *w,int content_h);
void fx_scrollbar_draw(fx_widget_t *w,int off,int content_h);
void fx_canvas_set_buf(fx_widget_t *w,int on);
#endif

/* ---- extra: 多尺寸文字 / 列表 / 下拉 ---- */
void *fxtk_font_size(int size);
int  fxtk_text_width_size(int size, const char *t);
void fxtk_draw_text_size(int size, int x, int y, const char *t, fx_color_t fg, fx_color_t bg);
fx_widget_t *fx_list_new(const char *r1, const char *r2);
void fx_list_add(fx_widget_t *w, const char *t);
void fx_list_set_cb(fx_widget_t *w, void (*cb)(fx_widget_t*, void*));
int  fx_list_sel(fx_widget_t *w);
fx_widget_t *fx_drop_new(const char *r1, const char *r2);
void fx_drop_add(fx_widget_t *w, const char *t);
void fx_set_fontsize(fx_widget_t *w, int size);
void fx_set_align(fx_widget_t *w, int a);
fx_widget_t *fx_list_new_p(const char *r1, const char *r2, int pg);
fx_widget_t *fx_drop_new_p(const char *r1, const char *r2, int pg);
int  fxtk_ui_scale(void);
void fxtk_set_ui_scale_cap(int p);   /* 行高/字号缩放上限% */          /* UI 缩放百分比, 480 设计宽=100 */
int  fxtk_font_height(int size);  /* 缩放后字高 */
int  fxtk_drv_width(void);
int  fxtk_drv_height(void);

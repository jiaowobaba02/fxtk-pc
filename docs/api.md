# fxtk API 参考

> 以 `components/fxtk/fxtk.h` 为最终清单。颜色为 RGB565。

## 属性宏

| 宏 | 说明 |
|---|---|
| `pixel("x1,y1","x2,y2")` | 480×272 设计坐标，等比缩放 |
| `percent("a,b","c,d")` | **0.0~1.0 浮点**，相对父控件 |
| `grid("name",r1,c1,r2,c2)` | 命名网格的格子区间 |
| `title(s)` / `name(s)` | 文本 / 控件名 |
| `call(fn)` | 回调 `void fn(fx_widget_t*,void*)` |
| `color(c)` / `fgcolor(c)` | 背景 / 前景色 |
| `line(n)` | 字号 |
| `row(n)` | 对齐 0左 1中 2右 |
| `value(n)` | 初始值 |
| `maxlen(n)` | 输入框限长 |
| `anim(n)` | 1=每帧重绘 |
| `page(n)` | 挂到 tab 第 n 页 |

## 控件创建

```c
fx_widget_t *fx_label_new(...);     /* 文本 */
fx_widget_t *fx_button_new(...);    /* 按钮 */
fx_widget_t *fx_canvas_new(...);    /* 画布(立即式回调) */
fx_widget_t *fx_slider_new(...);    /* 滑杆 */
fx_widget_t *fx_progress_new(...);  /* 进度 */
fx_widget_t *fx_checkbox_new(...);  /* 复选 */
fx_widget_t *fx_textedit_new(...);  /* 输入框 */
fx_widget_t *fx_list_new_p("r1","r2",rows);  /* 列表 */
fx_widget_t *fx_drop_new_p("r1","r2",page);  /* 下拉 */
fx_widget_t *fx_tab_new(...);  fx_widget_t *fx_panel_new(...);
fx_widget_t *fx_grid_new(...); fx_widget_t *fx_image_new(...);
```

示例：

```c
fx_button_new(pixel("340,40","400,70"), name("br_r"), page(6),
              title("红"), color(FX_RGB(244,67,54)), call(on_brush));
fx_widget_t *d = fx_drop_new_p("220,106","360,132", 10);
fx_drop_add(d,"字体: 小"); fx_list_set_cb(d, on_drop);
```

## 通用操作

```c
fx_widget_t *fx_find(const char *name);
void fx_parent(fx_widget_t *p);                 /* 设默认父, NULL=根 */
void fx_set_title(fx_widget_t*, const char*);
void fx_set_cb(fx_widget_t*, void(*)(fx_widget_t*,void*), void*);
void fx_widget_rect(fx_widget_t*, int*,int*,int*,int*);
void fx_widget_set_rect(fx_widget_t*, int x1,int y1,int x2,int y2);
void fx_repaint(void);
void fx_repaint_rect(int x1,int y1,int x2,int y2);
int  fxtk_ui_scale(void);                        /* 480宽=100 */
```

## 绘制原语（canvas 回调内，本地坐标）

```c
void fx_set_color(uint16_t);
void fx_fill_rect(int x1,int y1,int x2,int y2);
void fx_draw_rect(int x1,int y1,int x2,int y2);
void fx_draw_hline(int x1,int x2,int y);
void fx_draw_vline(int x,int y1,int y2);
void fx_draw_line(int x1,int y1,int x2,int y2);
void fx_draw_circle(int cx,int cy,int r);
void fx_draw_text_c(int x,int y,const char*,uint16_t fg,uint16_t bg);
void fxtk_draw_text_size(int size,int x,int y,const char*,uint16_t fg,uint16_t bg);
int  fx_text_width(const char*);
int  fxtk_text_width_size(int size,const char*);
```

## 文本

```c
void fx_set_fontsize(fx_widget_t*, int size);
void fx_set_align(fx_widget_t*, int a);   /* 0/1/2 */
```

## 输入

```c
void fx_touch_state(int *x,int *y,int *pressed);  /* 悬停也实时 */
fx_widget_t *fx_pressed(void);
fx_keyev_t fx_last_key(void);   /* .utf8 / .key(FX_KEY_*) */
```

## 列表/下拉数据

```c
void fx_list_add(fx_widget_t*, const char*);
void fx_drop_add(fx_widget_t*, const char*);
void fx_list_set_cb(fx_widget_t*, void(*)(fx_widget_t*,void*));
```

## 图片/特效

```c
void fx_set_image(fx_widget_t*, fx_image_t*);
void fx_image_set_zoom(fx_widget_t*, int pct);      /* 10~400 */
void fx_draw_image_rot(fx_image_t*, int cx,int cy,int deg,int pct);
void fx_image_grayscale(fx_image_t*);
```


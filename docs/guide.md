# fxtk 指南（guide.md）

> 本指南面向 **纯 PC / Linux 模拟器** 环境（已完全脱离 ESP32）。
> 以 `components/fxtk/fxtk.h` 为最终函数清单依据；本文覆盖全部常用公开 API，每个函数给出**用法 + 示例**。

---

## 1. 总体模型

fxtk 是一个 **“保留式控件 + 立即式绘制”** 混合的轻量 GUI：

- **控件（widget）是保留式的**：创建一次，长期存在，由核心统一布局、统一绘制、统一输入分发。
- **画布（canvas）内容是立即式的**：你给 canvas 挂一个回调，每帧（或脏区重绘时）核心调用它，你在里面用绘图原语“立即”画。
- **无操作系统依赖**：核心只依赖一个显示/输入驱动（`fx_sdl_driver`）。PC 上由 SDL 驱动实现。

一个最小应用的结构：

```c
/* 建界面：在 build_desktop_pages() 里创建控件 */
void build_desktop_pages(void);

/* 画布回调：立即式绘制 */
static void on_wave(fx_widget_t *w, void *ud) {
    int x1,y1,x2,y2; fx_widget_rect(w,&x1,&y1,&x2,&y2);
    int cw=x2-x1+1, ch=y2-y1+1;
    fx_set_color(FX_BLACK); fx_fill_rect(0,0,cw-1,ch-1);
    fx_set_color(FX_YELLOW);
    for(int x=0;x<cw;x+=2)
        fx_draw_vline(x, x, ch/2 + sin((x+s_phase)*0.1f)*ch/3);
}
```

---

## 2. 坐标系统与自动缩放

### 2.1 设计基线

- 设计基线为 **480×272**（`FX_DESIGN_W/H`）。
- 核心按窗口实际尺寸计算比例 `s_sx1000 = width*1000/480`、`s_sy1000 = height*1000/272`，所有 `pixel()` 坐标按此**等比（分数）缩放**，因此界面在任何分辨率下都铺满且比例一致。

### 2.2 三种布局模式

| 模式 | 宏 | 说明 |
|---|---|---|
| 像素 | `pixel("x1,y1","x2,y2")` | 480×272 设计坐标，等比缩放 |
| 百分比 | `percent("p1,p2","p3,p4")` | 0~1 百分比，相对父控件 |
| 网格 | `grid("名字",r1,c1,r2,c2)` | 挂在命名 grid 的格子区间 |

```c
fx_label_new(pixel("6,36","240,56"), page(5), title("你好"));
fx_canvas_new(percent("0,0","1,1"), name("full"));   /* 铺满父控件 */
```

### 2.3 `int fxtk_ui_scale(void)`

返回 UI 缩放百分比（480 设计宽 = 100）。用于**自绘内容**里需要随 UI 缩放的尺寸（行高、图标等）。

```c
int rh = 20 * fxtk_ui_scale() / 100;   /* 20px 行高随 UI 缩放 */
if (rh < 12) rh = 12;
```

---

## 3. 颜色系统

颜色为 **RGB565**（`uint16_t`）。

### `FX_RGB(r,g,b)`

```c
fx_set_color(FX_RGB(33,150,243));      /* 任意 8bit 分量转 RGB565 */
```

常用常量：`FX_BLACK`、`FX_WHITE`、`FX_GRAY`、`FX_LGRAY`、`FX_YELLOW`、`FX_GREEN`、`FX_RED` 等。

---

## 4. 控件创建 API

所有 `_new` 均为**变参属性列表**风格，属性用宏构造，顺序任意，以不同宏区分类型。

### 4.1 通用属性宏

| 宏 | 作用 |
|---|---|
| `pixel(a,b)` / `percent(a,b)` / `grid(...)` | 布局矩形 |
| `title("...")` | 标题/文本 |
| `name("...")` | 控件名（供 `fx_find` 查找） |
| `call(fn)` | 回调（绘制/事件） |
| `color(c)` | 背景色 |
| `fgcolor(c)` | 前景色 |
| `line(n)` | 字号（label 等） |
| `row(n)` | 对齐：0 左 / 1 中 / 2 右 |
| `value(n)` | 初始值（slider/progress） |
| `maxlen(n)` | 输入框最大长度 |
| `anim(1)` | 画布每帧重绘（动画） |
| `page(n)` | 挂到第 n 页（tab 的子页） |

### 4.2 `fx_label_new(...)`

文本标签。`line()` 设字号，`row()` 设对齐，`fgcolor()` 设文字色。

```c
fx_label_new(pixel("8,30","472,50"), page(10), line(15), row(1),
             title("居中文字"), fgcolor(FX_RGB(51,51,51)));
```

### 4.3 `fx_button_new(...)`

按钮。`call()` 在点击时触发。

```c
fx_button_new(pixel("340,40","400,70"), name("br_r"), page(6),
              title("红"), color(FX_RGB(244,67,54)), call(on_brush));
```

### 4.4 `fx_canvas_new(...)`

画布。`call()` 为绘制回调（立即式）。`anim(1)` 表示每帧重绘。

```c
fx_canvas_new(pixel("6,32","444,236"), name("wave"), page(0),
              anim(1), color(FX_BLACK), call(on_wave));
```

### 4.5 `fx_slider_new(...)`

滑杆。`value()` 初始值，`call()` 值变化回调。

```c
fx_slider_new(pixel("220,180","360,194"), page(10), value(44), call(on_nc_scale));
```

### 4.6 `fx_progress_new(...)`

进度条。`value()` 0~100。

```c
fx_progress_new(pixel("100,180","260,194"), name("mv_prog"), page(8), value(0));
```

### 4.7 `fx_checkbox_new(...)`

复选框。`title()` 为右侧文字。

```c
fx_checkbox_new(pixel("100,200","210,222"), name("mv_chk"), page(8),
                title("勾选机"), fgcolor(FX_WHITE));
```

### 4.8 `fx_textedit_new(...)`

单行输入框。`maxlen()` 限长，支持键盘输入与 Ctrl+C/V/X/A。

```c
fx_textedit_new(pixel("6,60","444,140"), name("edit1"), page(5), title("hello 你好"));
```

### 4.9 `fx_list_new_p("r1","r2",rows)` / `fx_list_new(...)`

列表。`rows` 为可见行数。配合 `fx_list_add` / `fx_list_set_cb`。

```c
fx_widget_t *list = fx_list_new_p("8,106","200,236", 10);
for (int i=0;i<12;i++) fx_list_add(list, names[i]);
fx_list_set_cb(list, on_nc_pick);
```

### 4.10 `fx_drop_new_p("r1","r2",page)` / `fx_drop_new(...)`

下拉框。`fx_drop_add` 加选项；靠底部自动向上弹；弹层带滚动不越界。

```c
fx_widget_t *drop = fx_drop_new_p("220,106","360,132", 10);
fx_drop_add(drop,"字体: 小"); fx_drop_add(drop,"字体: 中"); fx_drop_add(drop,"字体: 大");
fx_list_set_cb(drop, on_nc_drop);
```

### 4.11 容器类

- `fx_tab_new(...)`：标签页容器，子页用 `page(n)` 挂载。
- `fx_panel_new(...)`：面板容器。
- `fx_grid_new(...)`：网格容器，子件用 `grid()` 挂格。
- `fx_image_new(...)`：图片控件。

---

## 5. 控件通用操作

### `fx_widget_t *fx_find(const char *name)`

按名字查控件。

```c
fx_widget_t *tab = fx_find("tab");
```

### `void fx_parent(fx_widget_t *p)`

设置后续创建控件的**默认父控件**。`fx_parent(NULL)` 回到根。

```c
fx_parent(fx_find("tab"));   /* 之后创建的挂到 tab 下 */
```

### `void fx_set_title(fx_widget_t *w, const char *t)`

改标题并自动重绘。

```c
fx_set_title(w, "被点 3 次");
```

### `void fx_widget_set_rect(fx_widget_t *w,int x1,int y1,int x2,int y2)`

直接改控件矩形（屏幕坐标），自动触发旧/新区域重绘。

```c
fx_widget_set_rect(w, x+dx, y+dy, x+dx+w-1, y+dy+h-1);   /* 拖动 */
```

### `void fx_widget_rect(fx_widget_t *w,int *x1,*y1,*x2,*y2)`

取控件当前屏幕矩形（回调里用于得到本地宽高）。

```c
int x1,y1,x2,y2; fx_widget_rect(w,&x1,&y1,&x2,&y2);
int cw=x2-x1+1, ch=y2-y1+1;
```

### `void fx_set_cb(fx_widget_t *w, void (*cb)(fx_widget_t*,void*), void *ud)`

设置/替换回调。

### `void fx_repaint(void)` / `void fx_repaint_rect(x1,y1,x2,y2)`

全量/局部重绘请求。自绘内容变化后调用。

```c
fx_repaint_rect(w->x1,w->y1,w->x2,w->y2);
```

---

## 6. 绘制原语（canvas 回调内使用）

坐标均为**控件本地坐标**（0,0 为控件左上）。

| 函数 | 说明 |
|---|---|
| `fx_set_color(c)` | 设当前色 |
| `fx_fill_rect(x1,y1,x2,y2)` | 实心矩形 |
| `fx_draw_rect(x1,y1,x2,y2)` | 矩形边框 |
| `fx_draw_hline(x1,x2,y)` | 水平线 |
| `fx_draw_vline(x,y1,y2)` | 垂直线 |
| `fx_draw_line(x1,y1,x2,y2)` | 任意直线 |
| `fx_draw_circle(cx,cy,r)` | 圆 |
| `fx_draw_text_c(x,y,s,fg,bg)` | 文本（当前基础字号） |
| `fxtk_draw_text_size(size,x,y,s,fg,bg)` | 指定字号文本 |
| `fx_text_width(s)` / `fxtk_text_width_size(size,s)` | 测宽 |

示例（居中文字）：

```c
int tw = fxtk_text_width_size(16, "你好");
fxtk_draw_text_size(16, (cw-tw)/2, 10, "你好", FX_WHITE, FX_BLACK);
```

---

## 7. 文本与字体

- 基础字体由驱动打开；`fxtk_draw_text_size` 按需开指定字号（内部有**字体缓存**，满则淘汰，不会泄漏句柄）。
- `void fx_set_fontsize(fx_widget_t *w,int size)`：改 label 字号。
- `void fx_set_align(fx_widget_t *w,int a)`：改 label 对齐（0/1/2）。

```c
fx_set_fontsize(label, 20);
fx_set_align(label, 1);
```

---

## 8. 输入

### `void fx_touch_state(int *x,int *y,int *pressed)`

取鼠标/触摸当前坐标与按压状态。**悬停（未按下）也实时更新**。

```c
int mx,my,mp; fx_touch_state(&mx,&my,&mp);
```

### `fx_widget_t *fx_pressed(void)`

返回当前按下的控件（用于自绘按压态/拖动）。

### `fx_keyev_t fx_last_key(void)`

最近一次按键（含 `utf8` 与功能键枚举）。

```c
fx_keyev_t k = fx_last_key();
if (k.utf8[0]) ... else if (k.key==FX_KEY_RETURN) ...
```

### 滚轮

由驱动累积 `g_wheel_pix`，列表/滚动页消费；应用层一般无需直接处理。

---

## 9. 列表 / 下拉数据与回调

- `void fx_list_add(fx_widget_t *w,const char *t)`：加一行。
- `void fx_drop_add(fx_widget_t *w,const char *t)`：加一选项。
- `void fx_list_set_cb(fx_widget_t *w, void (*cb)(...))`：选中回调，回调内用控件的 `value`/`sel` 取选中索引。

```c
static void on_nc_pick(fx_widget_t *w,void*ud){
    /* w->value 为选中行号 */
}
```

---

## 10. 图片与特效（fxtk_effects.h）

- `void fx_set_image(fx_widget_t *w, fx_image_t *img)`：给 image 控件设图。
- `void fx_image_set_zoom(fx_widget_t *w,int p)`：缩放百分比 10~400。
- `void fx_draw_image_rot(fx_image_t *img,int cx,int cy,int angle_deg,int scale_pct)`：旋转+缩放贴图。
- `void fx_image_grayscale(fx_image_t *img)`：灰度化。

```c
fx_image_grayscale(s_pics[1]);
fx_draw_image_rot(img, cx, cy, 30, 100);
```

---

## 11. 重绘与性能模型

- **脏区重绘**：默认只重画变化矩形，`fx_repaint_rect` 精确触发。
- **全量重绘**：`fx_repaint()`；窗口 resize、切页等自动全量。
- **动画画布**：`anim(1)` 的 canvas 每帧重绘；其余控件脏时才画。
- **文本缓存**：渲染结果按 (字体,文本,fg,bg) 缓存为纹理，LRU 淘汰。
- 任何重绘请求当前都会提升为全量（保证无残影），性能仍为 GPU 立即式，60fps。

---

## 12. 弹层与右键菜单

- 下拉弹层、右键菜单均为**顶层弹层**，绘制在所有控件之上，不被页面裁剪。
- 下拉靠底部自动向上弹；弹层内容超出时带滚动，不越界。
- 点击弹层外自动关闭。

---

## 13. 完整示例：一页“波形+控制”

```c
static void on_wave(fx_widget_t *w,void*ud){
    int x1,y1,x2,y2; fx_widget_rect(w,&x1,&y1,&x2,&y2);
    int cw=x2-x1+1, ch=y2-y1+1;
    fx_set_color(FX_BLACK); fx_fill_rect(0,0,cw-1,ch-1);
    fx_set_color(FX_YELLOW);
    for(int x=0;x<cw;x+=2){
        int y=ch/2 + (int)(sinf(x*0.05f+s_t)*ch/3);
        fx_draw_vline(x, y, y);
    }
}
static void on_speed(fx_widget_t *w,void*ud){ s_speed=w->value; }

void build_wave_page(void){
    fx_label_new(pixel("6,6","200,24"), page(0), line(14), title("波形"));
    fx_canvas_new(pixel("6,32","444,200"), page(0), anim(1), color(FX_BLACK), call(on_wave));
    fx_label_new(pixel("6,206","40,220"), page(0), line(12), title("速度"));
    fx_slider_new(pixel("44,206","300,220"), page(0), value(30), call(on_speed));
}
```

---

## 14. 常见坑

1. **回调里坐标是本地坐标**：从 0 开始，不要加控件屏幕位置。
2. **`anim(1)` 才每帧画**：静态画布改内容后要手动 `fx_repaint_rect`。
3. **`page(n)` 必须配合 tab**：否则控件不随页切换显隐。
4. **改矩形用 `fx_widget_set_rect`**：直接改 `w->x1` 不会触发重绘。
5. **字号用 `line()`/`fx_set_fontsize`**：不要用自绘缩放字号，否则高分屏不清晰。

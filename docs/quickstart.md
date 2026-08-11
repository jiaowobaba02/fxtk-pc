# fxtk 快速上手（PC / Linux 模拟器）

## 构建与运行

在 `linux_sim` 目录执行 `build.sh`，然后运行生成的 `fxtk_sim`。窗口可任意拉伸，界面等比铺满。

## 三步写一个界面

**1. 建控件**（在 `build_desktop_pages()` 或自建函数中）：

```c
fx_label_new(percent("0.02,0.01","0.98,0.09"),
             title("我的应用"), fgcolor(FX_RGB(51,51,51)));
fx_button_new(pixel("100,100","220,136"), name("btn"),
              title("点我"), call(on_click));
```

**2. 写回调**：

```c
static void on_click(fx_widget_t *w, void *ud) {
    static int n = 0;
    char b[32]; snprintf(b, sizeof b, "被点 %d 次", ++n);
    fx_set_title(w, b);          /* 自动重绘 */
}
```

**3. 画布自绘**（立即式）：

```c
static void on_draw(fx_widget_t *w, void *ud) {
    int x1,y1,x2,y2; fx_widget_rect(w,&x1,&y1,&x2,&y2);
    int cw=x2-x1+1, ch=y2-y1+1;
    fx_set_color(FX_BLACK); fx_fill_rect(0,0,cw-1,ch-1);
    fx_set_color(FX_YELLOW); fx_draw_circle(cw/2, ch/2, ch/3);
}
fx_canvas_new(pixel("6,32","444,236"), anim(1), call(on_draw));
```

## 布局选择

- 固定小控件 → `pixel()`（设计坐标，等比缩放）。
- 需要铺满/对齐父容器 → `percent()`（0.0~1.0）。
- 表格类 → `grid()`。

## 常用操作

- 改文字 `fx_set_title`；改矩形 `fx_widget_set_rect`；
- 查控件 `fx_find("name")`；切换父 `fx_parent(...)`；
- 读鼠标 `fx_touch_state`；读按键 `fx_last_key`。


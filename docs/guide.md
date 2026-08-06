# fxtk 上手教程

从零开始, 用 fxtk 做一个带按钮、滑条、画布的界面。假设你手上有
EYA ETSP32 板 + 4.3寸 ST6201 屏 + GT911 触摸 (本项目默认硬件)。

---

## 第 1 步: 认识硬件

| 外设 | 引脚 | 说明 |
|---|---|---|
| 屏幕 SPI | SCK=23 MOSI=19 CS=22 DC=14 RST=12 | mode 0, 26MHz |
| 背光 | GPIO2 (+ GPIO32 飞线备份) | 高电平开 |
| 触摸 | SDA=18 SCL=16 RST=4 INT=17 | 硬件 I2C 400kHz |
| BOOT 键 | GPIO0 | 低电平按下 |

屏幕驱动 (ST6201) 和触摸驱动 (GT911) 已经写好, 你不用管细节;
想深入了解踩坑记录看 [internals.md](internals.md)。

---

## 第 2 步: 最小工程

新建 `main/app.c`:

```c
#include "fxtk.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    /* 1. 初始化外设 */
    fx_gt911_init();                       /* 触摸 (失败不阻塞) */
    fx_st6201_driver.init();               /* 屏幕 */
    fx_st6201_driver.touch_read = fx_gt911_read;   /* 把触摸挂到驱动 */

    /* 2. 初始化库 */
    fx_init(&fx_st6201_driver);

    /* 3. 建界面 (下面几节往里加) */

    /* 4. 主循环: 触摸 + 渲染 */
    while (1) {
        fx_poll();
        vTaskDelay(pdMS_TO_TICKS(16));     /* ~60fps 节奏 */
    }
}
```

`fx_poll()` 每轮做两件事:
1. 读触摸 → 事件分发 (按下/拖动/抬起)
2. 整帧重绘 (banded 帧缓冲, 无撕裂)

---

## 第 3 步: 第一个按钮

```c
static void on_hello(fx_widget_t *w, void *ud)
{
    fx_set_title(w, "你好, fxtk!");
}

/* 在 app_main 的"建界面"处: */
fx_button_new(pixel("20,20","140,60"), title("点我"), call(on_hello));
```

语法拆解:

- `pixel("20,20","140,60")` — 位置属性: 左上 (20,20) 右下 (140,60), 相对父容器
- `title("点我")` — 按钮上显示的文字
- `call(on_hello)` — 点击回调 (函数指针, **不带括号**)

回调签名固定: `void fn(fx_widget_t *w, void *ud)`, `w` 是触发回调的控件。

> 为什么不是 `call(on_hello())`? C 语言里 `on_hello()` 是"调用函数"不是
> "传函数指针"。回调传指针是 C 的标准做法, 这是唯一的语法妥协。

---

## 第 4 步: 控件表 (gird) 与网格定位

```c
/* 建一个 5 行 3 列的网格, 占 (20,20)..(460,120) */
fx_gird_map(pixel("20,20","460,120"), line(5), row(3), name("gird"));

/* 网格里放按钮: 覆盖 第1行第2列 .. 第1行第3列 (行列从 1 数) */
fx_button_new(gird("gird", 1, 2, 1, 3), title("横跨两格"), call(on_hello));
```

要点:

- `line(5)` 是**行数**, `row(3)` 是**列数** (沿用你定义的名字)
- `gird("gird", r1, c1, r2, c2)` 是位置属性: 从 (r1,c1) 覆盖到 (r2,c2)
- 子控件会随网格自动分格, 网格移动/缩放时子控件跟着动
- 网格容器本身不响应点击, 点击命中它的子控件

### 删除

```c
fx_delete(gird("gird"));          /* 按名字删除整个网格 (连子控件一起) */
fx_delete(fx_wptr(w));            /* 按指针删除单个控件 */
```

---

## 第 5 步: 百分比定位 (自适应分辨率)

```c
/* 屏幕 10%..90% 区域建网格 (换成任何分辨率的屏都自动适配) */
fx_gird_map(percent("0.1,0.1","0.9,0.9"), line(3), row(3), name("g2"));

/* 屏幕宽度一半的按钮: 0..50% */
fx_button_new(percent("0.0,0.02","0.5,0.08"), title("半屏按钮"), call(on_hello));
```

规则: `percent("左上x,左上y","右下x,右下y")`, 数值 0.0~1.0, 相对父容器。
父容器是网格/面板时, 百分比相对**容器**而不是屏幕。

换屏流程: 新屏幕填 `fx_driver_t.width/height` → `fx_init` → UI 自动重排,
**一行代码都不用改** (前提: UI 全部用 percent/gird 定位)。

---

## 第 6 步: 滑条 + 进度条 + 复选框

```c
/* 滑条拖动 → 同步进度条 */
static void on_slider(fx_widget_t *w, void *ud)
{
    fx_set_value(fx_find("prog"), fx_get_value(w));
}

fx_slider_new(pixel("20,100","300,122"), name("sl"),
              value(30), call(on_slider));
fx_progress_new(pixel("20,130","300,146"), name("prog"), value(30));
fx_checkbox_new(pixel("20,156","180,178"), title("启用"),
                name("ck"), value(1), call(on_check));
```

- `value(n)` 初值 0-100; `fx_get_value(w)` 读当前值
- 滑条: 按住滑块拖动实时回调; 进度条: 只读显示
- 复选框: 点击切换, `fx_get_value` 返回 0/1
- `name("id")` 给控件起名, 之后用 `fx_find("id")` 找到它

---

## 第 7 步: 画布与矢量绘制

```c
/* 画布: call 回调每帧自动调用, 裁剪已设到画布区域 */
static void on_draw(fx_widget_t *w, void *ud)
{
    int x1, y1, x2, y2;
    fx_widget_rect(w, &x1, &y1, &x2, &y2);   /* 取画布矩形 */
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;

    fx_set_color(FX_RED);
    fx_fill_circle(cw / 2, ch / 2, 40);

    fx_set_color(FX_GREEN);
    fx_fill_rect_round(10, 10, 100, 50, 6);

    fx_set_color(FX_WHITE);
    fx_draw_text_c(10, 60, "矢量文本", FX_WHITE, FX_BLACK);
}

fx_canvas_new(pixel("20,180","460,260"), name("cv"), call(on_draw));
```

可用图元: `fx_draw_line / fx_draw_rect / fx_fill_rect / fx_draw_circle /
fx_fill_circle / fx_fill_ellipse / fx_draw_arc / fx_fill_triangle /
fx_fill_polygon / fx_fill_rect_round / fx_draw_text ...`

坐标相对画布左上角, 超出画布的像素自动裁剪。

---

## 第 8 步: 面板 (子控件容器)

```c
fx_panel_new(pixel("20,30","300,180"), name("p"), color(0x0841));

/* 子控件坐标相对面板 (0,0) = 面板左上角 */
fx_button_new(pixel("10,10","120,40"), title("面板内按钮"), call(on_hello));
fx_label_new(pixel("10,50","280,70"), title("面板里的文字"), fgcolor(FX_WHITE));
```

创建子控件时**父容器自动判定**:
- 位置属性是 `gird(...)` → 父 = 该网格
- 否则 → 父 = 最近创建的... 不对, 父 = 根容器!

面板子控件的挂载: 目前设计里, 非 gird 定位的控件都挂在根。要在面板内
放控件, 用 `gird` 思路或等 `panel` 容器属性扩展。**当前版本**: 面板只做
装饰 (背景+边框), 子控件定位请用 percent/pixel 相对根规划, 或用嵌套
gird 实现分组。

> 这是文档诚实说明: v1 的 panel 未做"子控件父容器"绑定, 分组用 gird。

---

## 第 9 步: 帧渲染与性能

fxtk 默认每轮 `fx_poll()` 整帧重绘 (banded 帧缓冲):

```
fx_frame_begin();   → 绘制进入 16 行 band 缓冲
... 所有控件绘制 ...
fx_frame_end();     → 逐 band 刷出 (每带一次 SPI 窗口+像素流)
```

- 优点: 每帧内容完整、无撕裂, 代码不用管局部刷新
- 代价: 全屏刷新受 SPI 带宽限制 (480x272x2B ≈ 261KB/帧, 26MHz ≈ 80ms → ~12fps)

省电/提速方案 (按需):

```c
fx_set_autorepaint(0);          /* 关掉自动整帧重绘 */
/* 之后只有调用 fx_repaint() 时才重绘; 或用立即模式局部画 */
```

---

## 第 10 步: 完整示例

把上面拼起来就是 `main/app.c`, 直接编译烧录:

```bash
cd fxtk
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

屏幕上会看到: 网格按钮 (点击计数) / 删除网格按钮 / 重建界面按钮 /
滑条+进度条联动 / 复选框切换画布 / 画布里的矢量图形。

---

## 常见问题

**Q: 控件没显示?**
检查位置属性: pixel 相对父容器; percent 是 0.0~1.0; 控件池满会返回 NULL
(串口有 "widget pool full" 日志)。

**Q: 点了没反应?**
回调没传对: 写 `call(函数名)`, 签名 `void fn(fx_widget_t*, void*)`。
确认控件矩形存在 (控件被别的控件盖住也会命中上层)。

**Q: 中文显示成方框?**
字库 7445 汉字, 生僻字缺字会画方框, 换常用字。

**Q: 想要更多控件/字号?**
控件绘制在 `fxtk_widgets.c`, 照着加一个 `fxtk_draw_xxx` + 枚举 + 分发即可;
字号参考 esp32-tester 的 ascii_gray24/32 扩展。

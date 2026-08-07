# fxtk 完全开发指南

> 面向 ESP32 的嵌入式 GUI 框架 + Linux PC 模拟器。
> 本指南覆盖：核心概念 → 全部控件 → 画布绘图 → 事件/焦点/滚轮 → 桌面扩展 →
> 渲染管线 internals → 图片/特效/光追 → 示例导读 → 实战流程 → 移植 → FAQ/血泪坑表。

---

## 0. 目录与构建

```
components/fxtk/   核心库（平台无关，ESP32 与 PC 共用）
  fxtk.c           核心：属性解析/控件树/布局/事件分发/帧调度
  fxtk_draw.c      光栅化原语（线/圆/多边形/文字 blit/裁剪）
  fxtk_widgets.c   各控件绘制
  fxtk_font.c      字体（PC=SDL_ttf 加载文泉驿；真机=点阵字库）
  fxtk_effects.c   旋转贴图/多边形旋转/图像后处理
  fxtk_image.*     RGB565 图像表面
  fxtk.h           公开 API
  fxtk_internal.h  内部结构（app 层不要 include）
  fxtk_desktop.h   桌面扩展（textedit/scroll/maxlen/fx_wheel_take…）
  fxtk_effects.h / fxtk_image.h
linux_sim/         PC 模拟器
  main_linux.c     main + SDL 初始化 + 可调窗口 + 调用 app_init()
  fxtk_sdl_driver.c  驱动（鼠标→touch、滚轮、键盘/IME、系统剪贴板、窗口标题）
  fxtk_image_sdl.c SDL2_image 加载 PNG/JPG
  app.c            演示主应用（10 个标签页）← 你的正式代码替换它
  app_desktop.c    桌面扩展演示页
  raymarch.* / gpu_raymarch.*  CPU 多线程 / GPU(GLSL) 光追
  build.sh / build_ex.sh / make_release.sh
  examples/        ex01~ex06 独立示例
docs/              文档
```

构建（Ubuntu）：
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev
cd linux_sim
./build.sh                 # 完整演示
./build_ex.sh ex01_hello   # 独立示例
./make_release.sh          # 打包正式版
```

---

## 1. 核心概念

### 1.1 程序结构
- `main()` 在 `main_linux.c`：**你只写 `void app_init(void)`**。
- 主循环每帧调用 `fx_poll()`：处理滚轮/触摸/键盘 → 脏区或全屏重绘 → anim 画布回调。
- 单线程模型：所有回调都在 `fx_poll` 内执行，无需加锁。

### 1.2 最小应用
```c
#include "fxtk.h"
static void on_btn(fx_widget_t *w, void *ud) { fx_set_title(w, "clicked!"); }
void app_init(void) {
    fx_set_window_title("main");
    fx_set_bg(FX_RGB(240,240,240));
    fx_button_new(pixel("60,80","200,120"), title("OK"),
                  color(FX_RGB(33,150,243)), call(on_btn));
}
```

### 1.3 坐标系统与响应式缩放
- **设计分辨率 480×272**。`pixel("x1,y1","x2,y2")` 写设计坐标，窗口任意拉伸时
  布局按 `窗口/480、窗口/272` 等比缩放（`s_sx1000/s_sy1000` 定点）。
- `percent("0.1,0.1","0.9,0.3")`：父容器比例（千分定点）。
- `grid("name", r1,c1,r2,c2)`：挂在 `fx_grid_map` 网格上，可跨格。
- 控件坐标 = 父原点 + 偏移×缩放；**子控件永远相对父容器**。

### 1.4 属性宏语法
创建 = `fx_<控件>_new(属性1, 属性2, ..., FX_ATTR_END)`（宏自动补 END）。
通用属性：`pixel/percent/grid/title/text/name/call/line/row/color/fgcolor/
border/radius/value/page/anim/maxlen/image/fx_wptr`。
- `color()` = 背景；`fgcolor()` = 前景（文字/勾/滑条块）。
- `name()` 供 `fx_find()` 运行时查找。
- `call(cb)` 回调，签名 `void cb(fx_widget_t *w, void *ud)`；`ud` 用 `fx_set_cb` 设置。
- `page(n)`：挂在 TAB 的第 n 页（0 起）。
- `anim(1)`：画布每帧重绘。

### 1.5 控件树与生命周期
- `fx_parent(w)` 设置后续创建的父容器；`NULL` 回到根。
- 控件池上限 `FX_MAX_WIDGETS`（内部宏，PC 已扩到 1024）；池满会
  `ESP_LOGE("widget pool full")` 并返回 NULL——**创建大量控件前留意**。
- `fx_delete(fx_wptr(w))` 递归删除子树。

---

## 2. 控件大全

### 2.1 按钮 button
```c
fx_button_new(pixel("20,20","140,60"), title("启动"), name("btn"),
              color(FX_RGB(33,150,243)), radius(6), call(on_btn));
```
按下有高亮；释放且命中才触发回调。运行时：`fx_set_title/fx_set_color_w`。

### 2.2 标签 label
```c
fx_label_new(pixel("20,80","300,100"), title("状态: 待机"),
             fgcolor(FX_RGB(60,60,60)));
```
- 默认 `bg=FX_BLACK` 表示**透明**（文字抗alias 背景取 `fx_get_bg()`）。
- 显式 `color(c)` 才会铺底。**主题切换时要么显式绑定背景，要么只换前景**。
- 文本上限 `title[128]`（超长截断，注意别在汉字中间截）。

### 2.3 滑条 slider / 进度条 progress
```c
fx_slider_new(pixel("20,120","300,140"), name("sl"), value(30), call(on_sl));
fx_progress_new(pixel("20,150","300,166"), name("pg"), value(0));
```
- 值域 0~100；滑条拖动实时触发回调；`fx_set_value/fx_get_value` 读写。

### 2.4 复选框 checkbox
`value` 非 0 = 勾选；点击自动翻转后再回调。文字色用 `fgcolor`。

### 2.5 网格键盘 grid
```c
fx_grid_map(pixel("20,20","260,200"), line(3), row(3), name("pad"));
fx_button_new(grid("pad",1,1,1,1), title("1"), call(on_key));  // r1,c1,r2,c2 可跨格
```

### 2.6 标签页 tab
```c
fx_tab_new(pixel("10,26","470,266"), name("tab"), title("页A,页B,页C"));
fx_button_new(..., page(0), ...);   // 子控件用 page() 归页
```
- 页数 = title 逗号数+1；页名支持中文（≤128 字节总长）。
- 切页自动：隐藏页控件不立即重绘（`widget_in_active_page` 保护，防残影）。

### 2.7 画布 canvas（立即模式，见 §3）

### 2.8 图片 image
```c
fx_image_t *img = fx_image_load("assets/pic.png");   // PC: SDL2_image
fx_image_new(pixel("20,20","200,150"), image(img), call(on_zoom));
fx_image_set_zoom(w, 150);                            // 10~400%
fx_set_image(w, other_img);
```
程序生成图像：`fx_image_create(w,h)` + `fx_image_set_px` + `fx_draw_image`。

### 2.9 输入框 textedit（桌面扩展，include fxtk_desktop.h）
```c
fx_textedit_new(pixel("10,45","470,110"), name("e1"), title("初始文本"));
fx_textedit_new(pixel("10,120","470,155"), name("e2"), title(""), maxlen(20));
```
能力清单：
- **无限长度**：动态缓冲自动扩容；`maxlen(n)` 按**字符**（中文=1）限制并显示 `n/max`。
- **自动换行**（行高 22，左边距 6）；超出高度可**滚轮滚动 + 右侧滚动条（可拖拇指）**。
- **点击定位光标、拖拽跨行框选**（先算行再算列）。
- 键盘：直接打字（含中文 IME）、Backspace（按字符删）、←→/Home/End。
- 快捷键：Ctrl+A 全选 / Ctrl+C 复制 / Ctrl+X 剪切 / Ctrl+V 粘贴 —— **走系统剪贴板**，
  可与桌面其他应用互贴。
- 焦点：点击获焦（蓝框+光标闪烁）；点其他控件/切页自动失焦。
- 读取内容：`fx_textedit_get(w)`。

### 2.10 滚动容器 scroll（include fxtk_desktop.h）
```c
fx_scroll_new(pixel("6,32","444,236"), name("scr"), color(FX_RGB(245,245,245)));
fx_parent(fx_find("scr"));
for (int i=0;i<60;i++) fx_label_new(pixel("10,?", "430,?"), ...); // 子控件可超出容器高度
fx_parent(fx_find("tab"));
fx_scroll_content(fx_find("scr"), 60*40+12);   // 必须告知内容总高
```
- 子控件照常 `pixel()` 布局（可以排得比容器高）；绘制时整体平移并**裁剪进容器**。
- 滚轮翻页、右侧滚动条拖拇指；`hit_test` 做反向偏移，**滚动后按钮仍点得中**。
- 备选方案（更稳）：画布自绘长列表 + `fx_wheel_take`（见 §5.3）。

---

## 3. 画布立即模式绘图

```c
static void on_cv(fx_widget_t *w, void *ud) {
    int x1,y1,x2,y2; fx_widget_rect(w,&x1,&y1,&x2,&y2);
    int cw=x2-x1+1, ch=y2-y1+1;          // 回调坐标原点在画布左上角
    fx_set_color(0x0841); fx_fill_rect(0,0,cw-1,ch-1);
    fx_set_color(FX_YELLOW); fx_draw_line(0,ch/2,cw-1,ch/2);
    fx_fill_circle(cw/2,ch/2,20);
    fx_draw_text_c(10,10,"hello",FX_WHITE,0x0841);
}
fx_canvas_new(pixel("10,10","470,262"), anim(1), color(0x0841), call(on_cv));
```

原语：`fx_set_color / fx_fill_rect / fx_draw_rect / fx_draw_line / fx_draw_hline /
fx_draw_vline / fx_draw_circle / fx_fill_circle / fx_fill_ellipse / fx_fill_triangle /
fx_fill_polygon / fx_draw_arc / fx_fill_rect_round / fx_draw_text_c / fx_text_width /
fx_draw_text_c_n / fx_text_width_n`（_n 系列按字节子串，用于选区高亮）。

- `anim(1)`：无脏区时每帧自动回调（动画/仪表盘）。
- `fx_canvas_enable_buf(w)`：离屏缓冲，防复杂回调撕裂。
- 文字 `fx_draw_text_c(x,y,s,fg,bg)` 的 `bg` 用于抗锯齿混色；
  传 `FX_BLACK` 且从 label 语义调用时按透明处理。
- 颜色为 **RGB565**：`FX_RGB(r,g,b)`；预置 `FX_WHITE/BLACK/GRAY/LGRAY/YELLOW/MAGENTA` 等。

---

## 4. 事件系统

### 4.1 指针（鼠标/触摸）
- 驱动把鼠标映射为 touch：press/move/release 三态，`fx_poll` 内分发。
- 回调里可用：`fx_pressed()`（当前按中的控件）、`fx_touch_state(&x,&y,&p)`（全局坐标+按下态，画板用它）。
- 拖动：slider 自动跟手；textedit 拖拽=框选；滚动条轨道（右缘 6px）按下=拖拇指。

### 4.2 滚轮路由规则
滚轮命中“指针下方最近的可滚动祖先”：
- `FX_W_SCROLL / FX_W_TEXTEDIT` → 核心直接改 `scroll_y`（`scroll_y -= dy*24`，
  **轮下=往下看**，与桌面习惯一致）。
- `FX_W_CANVAS` → 增量累给 `fx_wheel_take(w)`（取后清零），由画布自绘滚动。

### 4.3 键盘与剪贴板
- `SDL_TEXTINPUT` 收 IME 提交（一次可能多个汉字，核心缓冲 64 字节不切字）。
- `fx_last_key()` 返回最后事件（utf8/key/mod），键鼠监视页用它。
- 剪贴板经驱动的 `clip_set/clip_get` ↔ 系统剪贴板。

### 4.4 焦点模型
- 同一时刻只有一个焦点（`s_focus`），仅 textedit 可获焦。
- 获焦：点击 textedit / 释放时再次命中。失焦：点任何其他控件、切到非焦点页。
- 隐藏页的焦点不会收到键盘路由（页保护）。

---

## 5. 桌面扩展深入

### 5.1 编辑模型（caret/anchor）
- `caret`=光标字节偏移，`anchor`=选区锚点；`[min,max)` 为选区。
- 输入/Backspace 先删选区再操作；方向键移动并清选区。
- 换行表 `te_wrap` 按像素宽折行；光标命中 `te_caret_from_xy` 先行后列。

### 5.2 运行时移动控件
```c
fx_widget_set_rect(w, x1,y1,x2,y2);  // 擦旧+画新，适合动效/悬浮窗/toast
```
压测页在 anim 画布回调里用它驱动多个控件沿李萨如轨道移动（移动中仍可点击）。

### 5.3 画布自绘长列表（推荐范式）
```c
static int s_off = 0;
static void on_view(fx_widget_t *w, void *ud) {
    int cw, ch; /* rect */ 
    int row=36, total=100*row, maxs=total-ch; if(maxs<0)maxs=0;
    s_off -= fx_wheel_take(w) * 24;              // 注意 -= ，轮下=往下
    if (s_off<0)s_off=0; if (s_off>maxs)s_off=maxs;
    /* 只画可见行: first = s_off/row ... y = i*row - s_off */
    /* 滚动条: th=ch*ch/total; ty=s_off*(ch-th)/maxs */
}
```
优点：无子控件矩形参与，**原理上杜绝越界/残影**。

### 5.4 主题切换最佳实践
```c
/* 先改控件，fx_set_bg 放最后（全屏重绘压轴，防“半新半旧”撕裂） */
for (...) { fx_set_color_w(ws[i], bg); fx_set_fgcolor(ws[i], fg); }
fx_set_bg(bg);
```
（核心另有 `s_full` 加固版帧调度：`fx_repaint()` 的全屏请求不被脏区降级。）

---

## 6. 渲染管线与性能（internals）

帧调度（`fx_poll` 尾部）优先级：
1. `s_full`（若启用）→ 无条件全屏 `fxtk_draw_all()`；
2. `s_repaint && s_dirty_n>0` → 只重绘脏区（`redraw_region`，最多 8 块自动合并，
   合并溢出自动升级全屏）；
3. `s_autorepaint` → 每帧全屏（调试用）；
4. 否则 `fxtk_draw_canvases()` → 只跑 `anim(1)` 画布（省电）。

- `fx_repaint_rect` 只排队不画；相交脏区会合并成大块。
- `redraw_widget_now`：**叶子控件**（按钮/滑条/进度/复选/画布）立即覆盖重绘；
  **容器**（TAB/GRID/PANEL/LABEL）走脏区，避免把子控件擦掉。
- 光栅化在 CPU（与 ESP32 一致）；PC 端 SDL2 只负责呈现（GPU 加速渲染器）。
- 光追：`raymarch.c` pthread 行级并行（SDF+软阴影+AO+雾）；
  `gpu_raymarch.c` 可选 GLSL 通道（EGL pbuffer，核显跑同一场景）。

---

## 7. 图片与特效（fxtk_image.h / fxtk_effects.h）

```c
fx_draw_image(img, x, y, w, h);                 // 最近邻缩放 blit，遵守裁剪/离屏
fx_draw_image_rot(img, cx, cy, deg, scale_pct); // 逆映射旋转+缩放
fx_fill_polygon_rot(pts, n, cx, cy, deg);       // 矢量多边形旋转
fx_image_flip_x/y, fx_image_grayscale, fx_image_tint, fx_image_brightness
```
图像为 RGB565 表面；后处理一次性遍历像素，适合“滤镜按钮”。

---

## 8. 示例导读 examples/

| 示例 | 看点 |
|---|---|
| ex01_hello | 按钮/标签/复选框；主题联动（bg+fg 一起换，fx_set_bg 压轴） |
| ex02_widgets | grid 键盘；滑条↔进度条联动 |
| ex03_anim | anim 画布；正弦波 + `fx_fill_polygon_rot` |
| ex04_edit | textedit 全家桶：跨行框选/剪贴板/maxlen/滚轮 |
| ex05_scroll | 画布自绘 100 行 + `fx_wheel_take` + 滚动条 |
| ex06_img | `fx_image_create` 生成 + `fx_draw_image_rot` 旋转 |

运行：`./build_ex.sh ex03_anim`（关窗自动返回）。

---

## 9. 实战：写你的正式应用

1. 直接替换 `linux_sim/app.c`，保留 `void app_init(void)`（不要写 main）。
2. 按需 include：`fxtk.h` 必备；桌面扩展 `fxtk_desktop.h`；特效 `fxtk_effects.h`；
   图片 `fxtk_image.h`。
3. 布局先画草图，按 480×272 写 `pixel()`；标题/状态栏用 `percent()` 自适应。
4. 控件全部 `name()`，回调用 `fx_find` 取句柄；状态机用静态变量 + `fx_set_title/fx_set_value` 刷新。
5. 多页面用 TAB；长日志用 textedit（无限长+滚轮）或画布自绘列表。
6. `./build.sh` 热迭代；定稿后 `./make_release.sh` 封包。

---

## 10. ESP32 移植清单

核心完全平台无关，只需实现 `fx_driver_t`：
- `width/height`；帧开始/结束与像素/块写入（或整屏 flush）；
- `touch_read`（触摸屏/旋钮映射）；可选 `key_read/wheel_read/clip_set/clip_get`；
- 字体走 `cn_gray/ascii_gray` 点阵字库（PC 用 SDL_ttf 是特例）；
- `app.c` 原样移植（去掉 PC 专属 include）。

---

## 11. FAQ / 血泪坑总表（全部真实踩过）

| 症状 | 根因 | 解法 |
|---|---|---|
| `widget pool full (64)!` 后续控件消失 | 池上限 | 扩 `FX_MAX_WIDGETS` |
| 连续中文夹杂□ | IME 一次提交多字被 7 字节截断 | `utf8[64]` 完整拷贝 |
| 长标签末尾□ | `title` 截断切坏 UTF-8 | `title[128]`；超长自行换行 |
| 输入框字数到顶 | 共用 title 缓冲 | 动态 `text_buf`；要限制用 `maxlen` |
| 框选不能跨行 | 光标命中只算 x | `te_caret_from_xy` 先行后列 |
| 滚动方向反 | SDL 轮下=负 | 统一 `-= dy*24` / 画布 `s_off -= take*24` |
| 主题切换“浅窗+黑盒” | 全屏请求被脏区降级 | `fx_set_bg` 放最后 / `s_full` 加固 |
| 连点标签页内容被擦空 | 容器立即重绘擦掉子控件 | 容器走脏区（redraw_widget_now 分流） |
| 滚动页顶部残影 | SCROLL 绘制缺裁剪/还原/return | 三件套齐全 + 相交防御 |
| 控件变左缘细条 | `pixel` 第二角点 x 写错（x1==x2） | 检查两角点坐标 |
| 示例链接报 `undefined main` | 示例只有 app_init | build_ex 必须编 `main_linux.c` |
| `fx_textedit_new/maxlen` 隐式声明 | 宏在 desktop 头 | `#include "fxtk_desktop.h"` |
| 旋转贴图隐式声明 | 特效头 | `#include "fxtk_effects.h"` |
| 窗口标题改不动 | 标题串在**驱动**文件 | 改 `fxtk_sdl_driver.c` 或全目录 sed |
| “修好了又坏” | 旧整合脚本回退源码 | 只跑单个 fix 脚本；跑前 `pkill -9 fxtk_sim` |
| 截图是旧窗口 | 多实例并存 | `pkill -9 fxtk_sim` 后单实例验证 |

---

## 12. API 速查总表

创建：`fx_button_new fx_label_new fx_slider_new fx_progress_new fx_checkbox_new
fx_grid_map fx_canvas_new fx_tab_new fx_image_new fx_textedit_new fx_scroll_new`
属性：`pixel percent grid title text name call line row color fgcolor border radius
value page anim maxlen image fx_wptr`
运行时：`fx_find fx_set_title fx_set_value fx_get_value fx_set_color_w fx_set_fgcolor
fx_set_cb fx_set_visible fx_widget_rect fx_widget_set_rect fx_widget_type fx_widget_title
fx_scroll_content fx_set_image fx_image_set_zoom fx_parent fx_delete`
系统：`fx_init fx_poll fx_width fx_height fx_set_bg fx_get_bg fx_repaint fx_repaint_rect
fx_set_autorepaint fx_set_touch_debug`
桌面：`fx_set_focus fx_get_focus fx_focus_blink fx_textedit_get fx_touch_state fx_pressed
fx_last_key fx_wheel_take fx_set_window_title`
画布：见 §3。特效/图片：见 §7。


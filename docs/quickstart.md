# Quickstart

## 1. 依赖与编译
见 README。`build.sh` 编译核心 + SDL 驱动 + `app.c`。

## 2. 最小应用
```c
#include "fxtk.h"
static void on_btn(fx_widget_t *w, void *ud) { fx_set_title(w, "clicked!"); }
void app_init(void) {
    fx_set_bg(FX_RGB(240,240,240));
    fx_button_new(pixel("60,80","200,120"), title("OK"), color(FX_RGB(33,150,243)), call(on_btn));
}
```
`app_init` 在 `fx_init` 之后被 main 调用；主循环每帧调用 `fx_poll()`。

## 3. 坐标系统
- `pixel("x1,y1","x2,y2")`：基于 480x272 设计分辨率，窗口缩放时等比拉伸；
- `percent("0.1,0.1","0.9,0.3")`：父容器比例；
- `grid("name", r1,c1,r2,c2)`：网格跨格。

## 4. 画布动画
`fx_canvas_new(..., anim(1), call(cb))`：回调每帧执行，回调内用立即模式绘图，
坐标原点在画布左上角。`fx_canvas_enable_buf(w)` 开启离屏缓冲防撕裂。

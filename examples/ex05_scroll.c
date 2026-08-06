/** ex05_scroll — 画布自绘长列表 + 滚动条 */
#include "fxtk.h"
#include "fxtk_desktop.h"
#include <stdio.h>
static int s_off = 0;
static void on_view(fx_widget_t *w, void *ud) {
    int x1, y1, x2, y2; fx_widget_rect(w, &x1, &y1, &x2, &y2);
    int cw = x2 - x1 + 1, ch = y2 - y1 + 1;
    int row = 36, total = 100 * row, maxs = total - ch; if (maxs < 0) maxs = 0;
    s_off += fx_wheel_take(w) * 24;
    if (s_off < 0) s_off = 0; if (s_off > maxs) s_off = maxs;
    fx_set_color(FX_WHITE); fx_fill_rect(0, 0, cw - 1, ch - 1);
    for (int i = s_off / row; i < 100; i++) {
        int y = i * row - s_off; if (y > ch) break;
        if (i & 1) { fx_set_color(FX_RGB(225, 238, 252)); fx_fill_rect(0, y, cw - 1, y + row - 1); }
        char t[32]; snprintf(t, sizeof(t), "第 %d 项", i + 1);
        fx_draw_text_c(12, y + 9, t, FX_RGB(30, 30, 30), FX_WHITE);
    }
    int th = ch * ch / total; if (th < 20) th = 20;
    int ty = maxs > 0 ? (int)((long)s_off * (ch - th) / maxs) : 0;
    fx_set_color(FX_GRAY); fx_fill_rect(cw - 5, 2, cw - 2, ch - 2);
    fx_set_color(FX_RGB(33, 150, 243)); fx_fill_rect(cw - 5, 2 + ty, cw - 2, 2 + ty + th);
}
void app_init(void) {
    fx_set_bg(FX_RGB(240, 240, 240));
    fx_canvas_new(pixel("10,10", "470,262"), anim(1), color(FX_WHITE), call(on_view));
}

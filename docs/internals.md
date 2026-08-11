# fxtk 内部机制

## 模块划分

| 文件 | 职责 |
|---|---|
| `fxtk.c` | 控件树、布局、输入分发、重绘调度、弹层 |
| `fxtk_widgets.c` | 各控件绘制 |
| `fxtk_extra.c` | 列表/下拉/滚动等扩展控件 |
| `fxtk_font.c` | TTF 字体缓存 + 文本纹理缓存 |
| `fxtk_draw.c` | 绘制原语、`fxtk_text_blit`、裁剪 |
| `fxtk_effects.c` | 图像特效 |
| `fxtk_sdl_driver.c` | SDL2 窗口/渲染/输入/resize |

## 对象模型

`fx_widget_t` 含 `type/flags/pos_mode`、布局源（`ox/oy` 像素 或 `px` 百分比 或 grid 引用）、`title/name`、`cb`、`bg/fg`、`border/radius`、`lines`(字号)/`rows`(对齐)/`value`、`page`、父子/兄弟指针、计算后矩形 `x1..y2`。

## 布局与缩放

- 基线 480×272；`s_sx1000=width*1000/480`、`s_sy1000` 同理。
- `FX_POS_PIXEL`：坐标×比例；`FX_POS_PERCENT`：父矩形×浮点；`FX_POS_GRID`：命名网格。
- resize 时 `sdl_apply_size` 更新比例并 `fx_layout()`+全量重绘。

## 重绘模型

- 默认脏区；当前任何 `fx_repaint_rect` 提升为全量（`s_full=1`），保证无残影。
- 渲染到 `target` 纹理（尺寸=逻辑宽高），present 时整体拷贝到后备缓冲。
- resize 重建 `target` 并清屏，帧末 `SDL_RenderSetClipRect(NULL)` 防 clip 残留。

## 字体与文本缓存

- `fxtk_font_size(size)`：缓存 `FC_N=8` 个 TTF；满则**淘汰并关闭**旧字体（`tc_drop_font` 清理关联文本纹理），杜绝句柄泄漏。
- 文本纹理缓存 256 项，键为 `字体|文本|fg|bg`，LRU 淘汰。

## 输入

- 驱动 `touch_read` 每帧回报；`s_last_tx/ty` **始终更新**（悬停可用）。
- 按下/移动/释放分发到最顶层命中控件；弹层优先捕获。
- 键盘入队 `g_kq`；滚轮累积 `g_wheel_pix`。

## 弹层

下拉/右键菜单为顶层 overlay，最后绘制、不被页面裁剪；点击外部关闭；靠底自动向上弹。

## 驱动契约（`fx_sdl_driver`）

提供 `width/height`、`touch_read`、`blit_tex`、`frame_end` 等；核心不依赖具体平台，替换驱动即可移植。


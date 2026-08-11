# Examples 导读

| 示例 | 看点 |
|---|---|
| ex01_hello | 按钮/标签/复选框、`fx_set_title/fx_set_bg` |
| ex02_widgets | `grid()` 键盘、滑条-进度条联动、`fx_set_color_w` 改网格底色 |
| ex03_anim | `anim(1)` 画布、波形 + `fx_fill_polygon_rot` 旋转矢量 |
| ex04_edit | 输入框：框选/剪贴板/`maxlen`/滚轮、右键置顶菜单 |
| ex05_scroll | 画布自绘长列表 + `fx_wheel_take` + 滚动条 |
| ex06_img | `fx_image_create` + `fx_draw_image_rot` 旋转贴图 |
| ex07_snake | `anim(1)` 主循环 + `fx_last_key` 方向键 + 数组地图/碰撞/重开 |
| ex08_array_buttons | 循环批量建按钮 + `name()`/回调来源区分 + 批量操作 |
| ex09_lottery | 随机 + 滚动动画 + `fxtk_draw_text_size` 大字号 |
| ex10_extra | 列表/下拉/多字号三件套 `fx_list_new_p`/`fx_drop_add`/`fx_set_fontsize`，弹层不越界 |
| ex11_percent | `percent()`（0.0~1.0）铺满/对齐，与 `pixel()` 对照 |
| ex12_hover | `fx_touch_state` 悬停实时坐标 + 悬停高亮（未按也更新） |
| ex13_popup | 底部下拉自动向上弹 + 输入框右键置顶菜单（需 `fxtk_desktop.h`） |
| ex14_resize | 窗口拖拽布局跟随、resize 无残影 |
| ex15_textcache | 多字号混排 + 字号滑杆，验证字体缓存淘汰不闪断 |

运行：`./build_ex.sh ex03_anim`（把 `ex03_anim` 换成任意示例名，如 `ex12_hover`）

## 备注

- **ex13** 用到输入框构造宏 `fx_textedit_new`，它在 `fxtk_desktop.h`，故该例额外 `#include "fxtk_desktop.h"`。
- **ex02** 网格默认深色底，示例用 `fx_set_color_w` 改浅，演示公开 API 改控件色。
- **ex12/ex14** 的画布回调里用 `fx_widget_rect` 取本地宽高，坐标均为本地系。


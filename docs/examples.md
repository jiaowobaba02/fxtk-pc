# Examples 导读

| 示例 | 看点 |
|---|---|
| ex01_hello | 按钮/标签/复选框、`fx_set_title/fx_set_bg`、深浅色切换 |
| ex02_widgets | `grid()` 键盘、滑条-进度条联动、`fx_set_color_w` 改控件色 |
| ex03_anim | `anim(1)` 画布、波形 + `fx_fill_polygon_rot` 旋转矢量 |
| ex04_edit | 输入框：框选/剪贴板/`maxlen`/滚轮、右键置顶菜单 |
| ex05_scroll | 画布自绘长列表 + `fx_scroll_update` 核心滚动 + `fx_scrollbar_draw`（滑块可拖拽） |
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
| ex16_dynamic | 运行时动态创建控件：`fx_widget_fix` 固定坐标 + 指针缓存 + 正弦漂移 |
| ex17_tabs | 立体标签页 + `page()` 页闸：三页不同内容（按钮/动画/矢量） |
| ex18_defaults | 默认配色零配置：全部控件不写 `color()`/`fgcolor()` 也好看 |

运行：
```bash
./build_ex.sh              # 一次全构建并逐个运行全部示例
./build_ex.sh all --no-run # 只全构建不运行
./build_ex.sh ex03_anim    # 单个示例 (换成任意名字)
```

## 备注

- **ex13** 用到输入框构造宏 `fx_textedit_new`，它在 `fxtk_desktop.h`，故该例额外 `#include "fxtk_desktop.h"`。
- **ex16** 展示动态控件三要素：`fx_widget_fix`（防布局复位）、指针数组（免 `fx_find`）、帧率门控（≥30fps 才加）。
- **ex17** 的 canvas 在页 1 用 `anim(1)` 动画、页 2 不带动画（静态绘制仅首次/脏区）。
- **默认配色**：控件不指定颜色即为浅底深字（网格浅底浅线、按钮蓝底白字），ex18 全程零显式颜色。
- **ex12/ex14** 的画布回调里用 `fx_widget_rect` 取本地宽高，坐标均为本地系。

## 从示例到完整演示

`demo-main/app.c` + `app_desktop.c` 是 12 页综合演示（含 3D 光追、GPU 粒子、动态压测），运行 `./build.sh` 查看；文档见 `desktop.md`。

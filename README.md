# fxtk — 面向 ESP32 的嵌入式 GUI 框架 + Linux PC 模拟器

fxtk 是一个**单线程、脏区重绘、属性宏驱动**的轻量 GUI 框架。核心用纯 C 编写，
光栅化在 CPU 完成（与 ESP32 真机行为一致），PC 端通过 SDL2 驱动呈现，
并额外提供桌面级扩展（输入框/滚动/滚轮/剪贴板）与渲染引擎能力（贴图/旋转/光追演示）。

## 特性

- **声明式控件**：`fx_button_new(pixel(...), title(...), call(...))` 属性宏链
- **三种布局**：`pixel()`（480x272 设计坐标，窗口响应式等比缩放）/ `percent()` / `grid()`
- **控件集**：按钮 / 标签 / 滑条 / 进度条 / 复选框 / 网格键盘 / 画布 / 标签页 / 图片 / 输入框 / 滚动容器
- **画布立即模式**：线/圆/椭圆/三角/多边形/圆弧/圆角矩形/文字，支持离屏缓冲与动画标志 `anim(1)`
- **桌面扩展**：文本框（换行/跨行框选/Ctrl+A C V X 系统剪贴板/`maxlen` 计数/滚轮）、
  滚轮路由、滚动条拖拽、焦点管理、`fx_widget_set_rect` 运行时移动控件
- **渲染引擎**：`fx_image_*` RGB565 表面、`fx_draw_image_rot` 旋转贴图、图像后处理
  （翻转/灰度/染色/亮度）、多线程软件 Raymarching（SDF 软阴影/AO/雾）+ 可选 GPU(GLSL) 通道
- **性能**：脏区合并重绘、行级 pthread 并行光追、GPU 呈现（SDL2 加速渲染器）

## 目录结构

```
components/fxtk/   核心库 (fxtk.c/draw/widgets/font/effects + 头文件)
linux_sim/         PC 模拟器 (SDL2 驱动 / 演示 app / GPU 光追 / examples)
docs/              文档 (quickstart / api / desktop / effects / examples)
examples/          独立示例 ex01~ex06
```

## 快速开始 (Linux)

```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev
cd linux_sim
./build.sh                # 编译并运行完整演示 (10 个标签页)
./build_ex.sh ex01_hello  # 运行独立示例
./make_release.sh         # 打包正式版 fxtk-v1.0.tar.gz
```

## 演示页速览

波形(动画画布) / 图形(旋转贴图+矢量动效) / 控件(网格键盘) / 图片(缩放/切换) /
3D(CPU 多线程光追, 超频+GPU 开关) / 输入(文本框全家桶) / 画板(鼠标作画) /
键鼠(事件监视) / 压测(移动控件) / 滚动(长列表+滚动条)

## 文档

- [docs/quickstart.md](docs/quickstart.md) — 从零写一个应用
- [docs/api.md](docs/api.md) — 控件与 API 速查表
- [docs/desktop.md](docs/desktop.md) — 桌面扩展语义（焦点/选区/滚轮/滚动）
- [docs/effects.md](docs/effects.md) — 图片/特效/光追与 CPU/GPU 说明
- [docs/examples.md](docs/examples.md) — 示例导读

## 许可

MIT（示例与文档同许可）。

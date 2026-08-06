# 图片 / 特效 / 光追

## 架构说明
核心光栅化在 **CPU**（与 ESP32 真机一致），GPU 只负责最终纹理呈现。
因此“榨干性能”的路线是：CPU 多线程光追（pthread 行级并行）+ GPU 呈现。
可选 GLSL 通道（`gpu_raymarch.c`，EGL pbuffer + fragment shader）把同一 SDF
场景搬到核显上，`glReadPixels` 读回帧缓冲。

## 软件 Raymarching (`raymarch.c`)
- 场景：解析平面/球 + SDF 圆环，smin 平滑混合；
- 光照：软阴影、高光、距离雾、抖动去色带；
- 并行：`pthread` 按行切分，帧率 ≈ 单核 × 核数。

## 图片表面
`fx_image_t` 为 RGB565 表面；`fx_draw_image` 最近邻缩放 blit（遵守 clip/离屏）；
`fx_draw_image_rot` 逆映射旋转贴图；后处理函数一次性修改像素。

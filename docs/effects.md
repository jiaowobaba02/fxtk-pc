# fxtk 图像特效

特效作用于 `fx_image_t`（`px` 为 RGB565 缓冲，含 `w/h`）。

## 旋转 + 缩放贴画

```c
void fx_draw_image_rot(fx_image_t *img, int cx, int cy, int angle_deg, int scale_pct);
```
以 `(cx,cy)` 为中心，`angle_deg` 逆时针旋转，`scale_pct` 缩放（100=原尺寸）。用于 3D/旋转展示。

```c
fx_draw_image_rot(img, cw/2, ch/2, angle, 120);
```

## 灰度

```c
void fx_image_grayscale(fx_image_t *img);
```
原地置灰，常用于“禁用/未选”态。

## 缩放（image 控件）

```c
void fx_image_set_zoom(fx_widget_t *w, int pct);   /* 10~400 */
```

## 组合示例

```c
fx_image_grayscale(pic);              /* 先处理像素 */
fx_draw_image_rot(pic, x, y, 45, 80); /* 再旋转贴出 */
```

> 特效为纯 CPU 像素级，离屏画布（3D）走 `fxtk_put_px` 像素路径，与 GPU 文本路径互不干扰。


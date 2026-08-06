# API 速查

## 控件创建
| 控件 | 构造 | 常用属性 |
|---|---|---|
| 按钮 | `fx_button_new` | title/color/call |
| 标签 | `fx_label_new` | title/fgcolor |
| 滑条 | `fx_slider_new` | value/color/call |
| 进度条 | `fx_progress_new` | value |
| 复选框 | `fx_checkbox_new` | value/fgcolor/call |
| 网格 | `fx_grid_map` | line/row |
| 画布 | `fx_canvas_new` | anim/color/call |
| 标签页 | `fx_tab_new` | title("页1,页2") |
| 图片 | `fx_image_new` | image(ptr)/call |
| 输入框 | `fx_textedit_new` | title/maxlen |
| 滚动容器 | `fx_scroll_new` | color + `fx_scroll_content(w,h)` |

## 运行时
`fx_find / fx_set_title / fx_set_value / fx_get_value / fx_set_color_w / fx_set_fgcolor /
fx_set_cb / fx_set_visible / fx_widget_rect / fx_widget_set_rect / fx_set_bg / fx_parent /
fx_set_focus / fx_textedit_get / fx_touch_state / fx_pressed / fx_last_key / fx_wheel_take`

## 画布立即模式
`fx_set_color / fx_fill_rect / fx_draw_rect / fx_draw_line / fx_draw_hline / fx_draw_vline /
fx_draw_circle / fx_fill_circle / fx_fill_ellipse / fx_fill_triangle / fx_fill_polygon /
fx_draw_arc / fx_fill_rect_round / fx_draw_text_c / fx_text_width /
fx_draw_text_c_n / fx_text_width_n`

## 图片与特效 (fxtk_image.h / fxtk_effects.h)
`fx_image_create/free/set_px/load`、`fx_draw_image`、`fx_set_image`、`fx_image_set_zoom`、
`fx_draw_image_rot`、`fx_fill_polygon_rot`、`fx_image_flip_x/y`、`fx_image_grayscale`、
`fx_image_tint`、`fx_image_brightness`

# fxtk 内部架构与移植经验

本文面向想改 fxtk 源码 / 换屏幕 / 移植到别的板子的人。
分三部分: 渲染管线、控件系统、硬件移植踩坑。

---

## 1. 渲染管线

### 像素出口 (唯一落点)

所有绘制 (控件、矢量、文本) 最终都调用 `fxtk_put_px(x, y, color)`,
它在 `fxtk_draw.c`。两层过滤:

1. **裁剪**: 当前 clip 矩形 (`fx_set_clip`), 控件绘制时把 clip 收到自身矩形
2. **缓冲**: 帧模式进 band 缓冲, 立即模式进行缓冲

### banded 帧缓冲 (为什么不是全屏双缓冲)

ESP32 经典版 SRAM 只有 320KB, 全屏 RGB565 缓冲要 261KB, 放不下。
fxtk 的折中:

```
┌─ 帧模式 ──────────────────────────────┐
│ fx_frame_begin()                      │
│   绘制 → 写入 16 行 band 缓冲 (15KB)  │
│   遇到新 band → 旧 band 整带刷出      │
│ fx_frame_end() → 最后一带刷出         │
└───────────────────────────────────────┘
```

- 每带一次 `set_window + push_pixels` (SPI 窗口 + 像素流, CS 全程拉低)
- 带内内容完整, 无撕裂; 整帧自上而下快速逐带刷出, 视觉上等同整帧
- 换屏幕只需保证驱动实现 set_window/push_pixels

立即模式 (不调用 begin/end): 单行缓冲 (960B), 换行即刷, 适合局部小改。

### 性能参考 (480x272, SPI 26MHz)

- 全帧刷新: 261KB / 帧 → 理论 ~12fps
- 单行刷新: 960B → ~0.3ms
- 结论: 整帧渲染适合静态界面; 动态区域 (滑块/动画) 用立即模式局部画

---

## 2. 控件系统

### 控件树

```
s_root (全屏面板容器, 不可删除)
 ├─ gird "gird" (网格容器)
 │   └─ button (gird_ref → "gird", gr1..gc2 行列)
 ├─ button
 ├─ slider ...
```

- 控件池: 64 个静态 `fx_widget_t` (零 malloc), 用 `type == FX_W_NONE` 标记空闲
- 挂载规则: 位置属性是 `gird(...)` → 父 = 网格; 否则父 = 根
- 删除: 从链表断开 + 递归释放子控件 (池标记)

### 布局三模式

| 模式 | 原始值 | 布局公式 |
|---|---|---|
| PIXEL | ox1,oy1,ox2,oy2 | x = parent.x1 + ox ... |
| PERCENT | px1..py2 (千分比) | x = parent.x1 + parent.w * p / 1000 |
| GIRD | gird_ref + gr1..gc2 | x = gird.x1 + (c-1)*cellw ... |

布局在创建/删除后自动全量重算 (控件少, 便宜)。percent 是"自适应分辨率"
的核心: 分辨率来自驱动, 重算即可。

### 事件分发

```
touch_read (每轮状态) ──边沿检测──▶ press / move / release
                                        │
                                   hit_test (后序: 子优先)
                                        │
              press: 记录 s_pressed + FX_F_PRESSED
              move:  slider → value 更新 + 回调
              release: 同一控件 → cb(w, ud)
```

容器 (gird/panel) 不参与命中。回调里删除"触发回调的控件"有悬垂风险,
库内已做防护 (s_pressed 清理), 但不要依赖。

### 属性系统 (宏魔法)

```c
#define fx_button_new(...) fx_widget_new_impl(FX_W_BUTTON,
                              (fx_attr_t[]){__VA_ARGS__, FX_ATTR_END})
```

- C99 复合字面量数组: 变参 → 属性数组, 末尾 FX_ATTR_END 哨兵
- `gird()` 宏重载: 1 参 → gird1 (查找), 5 参 → gird5 (定位)
  `#define gird(...) FX_GIRD_SEL(__VA_ARGS__, gird5, gird5, gird5, gird5, gird1)(__VA_ARGS__)`
- `fx_attr_t` = tag + union, 属性构造器返回结构体 (按值)

### 加新控件三步

1. `fxtk.h`: 枚举 + 创建宏 (`#define fx_xxx_new(...)`)
2. `fxtk_internal.h`: 声明 `fxtk_draw_xxx(w)`
3. `fxtk_widgets.c`: 实现绘制; `fxtk.c` draw_widget 里加分发 + 默认主题

---

## 3. 硬件移植经验 (血泪踩坑)

### ST6201 屏幕 (fxtk_st6201.c)

从 esp32-tester 移植, 四件事必须做对, 否则黑屏/花屏:

1. **SPI mode 0, 26MHz**
   GPIO matrix 路由的 SPI 上限 26.6MHz。写 40MHz 会被驱动拒绝,
   或者时序不稳花屏。CLK=23 MOSI=19, 用 SPI2_HOST + DMA。

2. **像素大端 (高字节先)**
   ST6201 按大端解析 16bit 像素。写成小端 (常见习惯) 会
   R→B / G→R / B→G 通道错乱 (红蓝互换)。`push_pixels` 里
   `buf[j*2] = c >> 8; buf[j*2+1] = c & 0xFF;` 不能省。

3. **RAMWR 像素流 CS 全程拉低**
   设置窗口 (0x2A/0x2B/0x2C) 后开始推像素, CS 中途拉高一次就花屏。
   用软件 CS (spics_io_num = -1), 整段像素流期间保持低电平。

4. **初始化序列必须用官方版**
   0x36=0xC0 (扫描方向), 0x41=0x03 (BOE Gamma 相关), 序列里
   {0xFF,0xA5} 开头 {0xFF,0x00} 结尾。换成通用 ST7789/ILI9341 序列
   会显示异常 (颜色/方向/Gamma 全错)。序列在 fxtk_st6201.c 顶部,
   改屏时必须整段替换。

背光: GPIO2 高电平开 (这块板与 8080 版不同), GPIO32 是飞线备份。

### GT911 触摸 (fxtk_touch.c)

1. 地址 0x14 (7bit), 上电复位时序决定地址 (INT 拉高 → 0x14)
2. 状态寄存器 0x814E: bit7 = 有新数据, 低 4 位 = 点数;
   **读后要写 0 清除** (官方逻辑, 不清会死锁)
3. 坐标寄存器 0x8150: 每点 4 字节小端 (Xlo,Xhi,Ylo,Yhi)
4. **必须硬件 I2C (400kHz)**: 软件 I2C 不工作, GT911 会 clock stretching
5. 坐标直接映射, 无交换/镜像; 上限 0x8048/0x804A 出厂已配 480x272

### 换屏幕流程

1. 复制 `fxtk_st6201.c` 改: 初始化序列 + 分辨率 + 引脚 (set_window/push_pixels
   一般不用改, 都是标准 0x2A/0x2B/0x2C + 像素流)
2. 实现 `fx_driver_t` 实例 (width/height 填对)
3. UI 用 percent/gird 定位的话, 代码零改动

### 字库 (cn_gray / ascii_gray16)

- cn_gray: 16px 微软雅黑灰度, 2bit/像素 (4 级抗锯齿), unicode 有序 → 二分查找
- ascii_gray16: Arial 16px, 比例字形 (adv 前进量), yoff 相对基线
- 混排: 中文 17px 前进, ASCII 按 adv; 中英垂直对齐差 1px, 中文格顶下移 1px
  (CN_ALIGN_DY) 视觉对齐
- 像素解包: `(data[pix>>2] >> ((pix&3)<<1)) & 3` → alpha 表 {0,85,170,255}
- RGB565 混合: 分量 `(fg*a + bg*(255-a)) >> 8` (纯整数)
- 生成脚本在 esp32-tester 的 tools/ (gen_font*.py)

---

## 4. 内存账本 (480x272 屏)

| 用途 | 大小 |
|---|---|
| band 缓冲 (16x480x2) | 15 KB |
| 行缓冲 (480x2) | 960 B |
| 控件池 (64 x 结构体) | ~10 KB |
| 字体数据 (flash, 不占 RAM) | cn_gray 2.5MB + ascii 28KB |
| 静态总量 | ~30 KB RAM |

ESP32 有 320KB SRAM, 余量充足。AI/网络等功能可叠加。

---

## 5. 扩展路线 (TODO)

- [ ] panel 子控件父容器绑定 (嵌套容器)
- [ ] 多字号 (24/32px, 字库已有)
- [ ] 焦点/键盘输入 (esp32-tester 有现成软键盘可移植)
- [ ] 主题系统 (颜色表集中管理)
- [ ] 动画 (fade/slide, 基于 value 插值)
- [ ] 滚动容器 (scroll 控件)
- [ ] PSRAM 板全屏双缓冲 (直接改 fxtk_draw.c 的缓冲)

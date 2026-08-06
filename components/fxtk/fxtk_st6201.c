/**
 * fxtk_st6201.c — ST6201 SPI 屏幕驱动 (fxtk 驱动抽象实现)
 *
 * 从 esp32-tester 项目移植 (gfx_driver.c), 踩坑记录:
 *   1. SPI mode 0, 26MHz (GPIO matrix 路由上限 26.6MHz; 40MHz 会被拒绝)
 *   2. 像素大端 (高字节先) — 小端会导致 R→B/G→R/B→G 通道错乱
 *   3. RAMWR 像素流 CS 必须全程拉低 (每行拉高会花屏)
 *   4. 初始化序列必须用官方版 (0x36=0xC0, 0x41=0x03, BOE Gamma)
 *
 * 换屏幕: 实现 fx_driver_t 的四个函数 (init/set_window/push_pixels/hold),
 * 分辨率写进 width/height, 布局用 percent() 自动适配, 无需改库代码。
 */
#include "fxtk.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 硬件引脚 (ETSP32 板 + 4.3寸 480x272, 官方 EYA 走线) */
#define PIN_SCK   23
#define PIN_MOSI  19
#define PIN_CS    22
#define PIN_DC    14
#define PIN_RST   12

static const char *TAG = "fxtk_st6201";
static spi_device_handle_t s_spi;

/* ---------- 官方初始化序列 (0x36=0xC0, 0x41=0x03, BOE Gamma) ---------- */
static const uint8_t seq[][2] = {
    {0xFF,0xA5},{0xE7,0x10},{0x35,0x00},{0x36,0xC0},{0x3A,0x01},{0x40,0x01},
    {0x41,0x03},{0x44,0x15},{0x45,0x15},{0x7D,0x03},{0xC1,0xBB},{0xC2,0x05},
    {0xC3,0x10},{0xC6,0x3E},{0xC7,0x25},{0xC8,0x21},{0x7A,0x51},{0x6F,0x49},
    {0x78,0x57},{0xC9,0x00},{0x67,0x11},{0x51,0x0A},{0x52,0x7D},{0x53,0x0A},
    {0x54,0x7D},{0x46,0x0A},{0x47,0x2A},{0x48,0x0A},{0x49,0x1A},{0x44,0x15},
    {0x45,0x15},{0x73,0x08},{0x74,0x10},{0x56,0x43},{0x57,0x42},{0x58,0x3C},
    {0x59,0x64},{0x5A,0x41},{0x5B,0x3C},{0x5C,0x02},{0x5D,0x3C},{0x5E,0x1F},
    {0x60,0x80},{0x61,0x3F},{0x62,0x21},{0x63,0x07},{0x64,0xE0},{0x65,0x02},
    {0xCA,0x20},{0xCB,0x52},{0xCC,0x10},{0xCD,0x42},{0xD0,0x20},{0xD1,0x52},
    {0xD2,0x10},{0xD3,0x42},{0xD4,0x0A},{0xD5,0x32},
    {0x80,0x00},{0xA0,0x00},{0x81,0x06},{0xA1,0x08},{0x82,0x03},{0xA2,0x03},
    {0x86,0x14},{0xA6,0x14},{0x87,0x2C},{0xA7,0x26},{0x83,0x37},{0xA3,0x37},
    {0x84,0x35},{0xA4,0x35},{0x85,0x3F},{0xA5,0x3F},{0x88,0x0A},{0xA8,0x0A},
    {0x89,0x13},{0xA9,0x12},{0x8A,0x18},{0xAA,0x19},{0x8B,0x0A},{0xAB,0x0A},
    {0x8C,0x17},{0xAC,0x0B},{0x8D,0x1A},{0xAD,0x09},{0x8E,0x1A},{0xAE,0x08},
    {0x8F,0x1F},{0xAF,0x00},{0x90,0x08},{0xB0,0x00},{0x91,0x10},{0xB1,0x06},
    {0x92,0x19},{0xB2,0x15},{0xFF,0x00},
};

static int s_hold = 0;    /* 连续事务模式: CS 保持拉低 */

static void st_cmd(uint8_t cmd)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    if (!s_hold) gpio_set_level(PIN_CS, 0);
    gpio_set_level(PIN_DC, 0);
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(s_spi, &t);
    if (!s_hold) gpio_set_level(PIN_CS, 1);
}

static void st_data(const uint8_t *buf, size_t len)
{
    if (!s_hold) gpio_set_level(PIN_CS, 0);
    gpio_set_level(PIN_DC, 1);
    while (len) {
        size_t chunk = len > 4096 ? 4096 : len;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = chunk * 8;
        t.tx_buffer = buf;
        spi_device_transmit(s_spi, &t);
        buf += chunk;
        len -= chunk;
    }
    if (!s_hold) gpio_set_level(PIN_CS, 1);
}

static void st_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t b[4];
    st_cmd(0x2A);
    b[0]=x0>>8; b[1]=x0&0xFF; b[2]=x1>>8; b[3]=x1&0xFF; st_data(b, 4);
    st_cmd(0x2B);
    b[0]=y0>>8; b[1]=y0&0xFF; b[2]=y1>>8; b[3]=y1&0xFF; st_data(b, 4);
    st_cmd(0x2C);
}

/* 像素流: 大端 (高字节先); CS 全程低, 分块 DMA */
static void st_push_pixels(const uint16_t *buf, uint32_t n)
{
    static uint8_t big[8192];
    if (!s_hold) gpio_set_level(PIN_CS, 0);
    gpio_set_level(PIN_DC, 1);
    uint32_t i = 0;
    while (i < n) {
        uint32_t chunk_n = (n - i > 4096 / 2) ? 4096 / 2 : (n - i);
        for (uint32_t j = 0; j < chunk_n; j++) {
            big[j * 2]     = buf[i + j] >> 8;
            big[j * 2 + 1] = buf[i + j] & 0xFF;
        }
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = chunk_n * 16;
        t.tx_buffer = big;
        spi_device_transmit(s_spi, &t);
        i += chunk_n;
    }
    if (!s_hold) gpio_set_level(PIN_CS, 1);
}

static void st_hold_begin(void) { s_hold = 1; gpio_set_level(PIN_CS, 0); }
static void st_hold_end(void)   { s_hold = 0; gpio_set_level(PIN_CS, 1); }

/* 单色填充: 直接发大端字节流 (1 次窗口 + 分块传输, 比逐像素快一个数量级) */
static void st_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                         uint16_t color)
{
    uint32_t n = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
    static uint8_t buf[2048 * 2];              /* 4KB 大块减少事务数 */
    st_set_window(x0, y0, x1, y1);
    if (!s_hold) gpio_set_level(PIN_CS, 0);
    gpio_set_level(PIN_DC, 1);
    uint32_t i = 0;
    while (i < n) {
        uint32_t chunk = (n - i > 2048) ? 2048 : (n - i);
        for (uint32_t j = 0; j < chunk; j++) {
            buf[j * 2]     = color >> 8;       /* 高字节先 */
            buf[j * 2 + 1] = color & 0xFF;
        }
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = chunk * 16;
        t.tx_buffer = buf;
        spi_device_transmit(s_spi, &t);
        i += chunk;
    }
    if (!s_hold) gpio_set_level(PIN_CS, 1);
}

static int st_init(void)
{
    /* 背光: GPIO2 (板上 BCKL) + GPIO32 (飞线备份), 高电平开.
     * ⚠️ 漏了它屏幕会黑屏 (SPI 在跑但没背光) */
    gpio_config_t bl = {
        .pin_bit_mask = (1ULL << 2) | (1ULL << 32),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl);
    gpio_set_level(2, 1);
    gpio_set_level(32, 1);

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST) | (1ULL << PIN_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(PIN_CS, 1);

    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192,
    };
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK)
        return -1;

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 26000000,    /* GPIO matrix 上限 26.6MHz */
        .mode = 0,
        .spics_io_num = -1,            /* 软件 CS */
        .queue_size = 4,
    };
    if (spi_bus_add_device(SPI2_HOST, &dev, &s_spi) != ESP_OK)
        return -1;

    /* 复位 + 官方序列 + 开显示 */
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        st_cmd(seq[i][0]);
        uint8_t d = seq[i][1];
        st_data(&d, 1);
    }
    st_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    st_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "ST6201 480x272 init done");
    return 0;
}

/* ---------- fxtk 驱动实例 (触摸见 fxtk_touch.c 的 fx_gt911_read) ---------- */
fx_driver_t fx_st6201_driver = {
    .width = 480,
    .height = 272,
    .init = st_init,
    .set_window = st_set_window,
    .push_pixels = st_push_pixels,
    .hold_begin = st_hold_begin,
    .hold_end = st_hold_end,
    .fill_rect = st_fill_rect,
    .touch_read = NULL,    /* 由 app 填入 fx_gt911_read, 或使用组合驱动 */
};
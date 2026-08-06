/**
 * fxtk_touch.c — GT911 电容触摸驱动 (硬件 I2C, 从 esp32-tester 移植)
 *
 * 踩坑记录:
 *   1. GT911 地址 0x14 (7bit), 坐标上限出厂已配 480x272
 *   2. 状态寄存器 0x814E: bit7=有新数据, 低4位=点数; 读后需写 0 清除
 *   3. 坐标寄存器 0x8150 起: 每点 4 字节小端 (Xlo,Xhi,Ylo,Yhi)
 *   4. 硬件 I2C 400kHz 稳定; 软件 I2C 不工作 (GT911 clock stretching)
 *   5. 触摸坐标直接映射 (无交换/镜像)
 */
#include "fxtk.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 引脚 (ETSP32 板) */
#define TOUCH_SDA  18
#define TOUCH_SCL  16
#define TOUCH_RST  4
#define TOUCH_INT  17

static const char *TAG = "fxtk_touch";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static uint16_t s_xmax = 480, s_ymax = 272;
static int s_ready = 0;

static int gt911_r(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t r[2] = { reg >> 8, reg & 0xFF };
    return i2c_master_transmit_receive(s_dev, r, 2, buf, len, 100) == ESP_OK ? 0 : -1;
}

static int gt911_w8(uint16_t reg, uint8_t val)
{
    uint8_t b[3] = { reg >> 8, reg & 0xFF, val };
    return i2c_master_transmit(s_dev, b, 3, 100) == ESP_OK ? 0 : -1;
}

/* 初始化: 0=成功 (app 在 fx_init 前调用) */
int fx_gt911_init(void)
{
    /* 复位 GT911 (地址由 INT 电平决定) */
    gpio_config_t rst = {
        .pin_bit_mask = (1ULL << TOUCH_RST) | (1ULL << TOUCH_INT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst);
    gpio_set_level(TOUCH_RST, 0);
    gpio_set_level(TOUCH_INT, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TOUCH_INT, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_direction(TOUCH_INT, GPIO_MODE_INPUT);

    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bc, &s_bus) != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus init fail");
        return -1;
    }

    static const uint8_t addrs[] = { 0x5D, 0x14 };
    for (int i = 0; i < 2; i++) {
        i2c_device_config_t dc = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addrs[i],
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(s_bus, &dc, &s_dev) != ESP_OK)
            continue;
        uint8_t id[4] = { 0 };
        if (gt911_r(0x8140, id, 4) == 0 && id[0] == '9' && id[1] == '1') {
            ESP_LOGI(TAG, "GT911 @0x%02X id=%c%c%c", addrs[i], id[0], id[1], id[2]);
            uint8_t b[4];
            if (gt911_r(0x8048, b, 4) == 0) {
                uint16_t xm = b[0] | (b[1] << 8), ym = b[2] | (b[3] << 8);
                if (xm > 16 && xm < 4096) s_xmax = xm;
                if (ym > 16 && ym < 4096) s_ymax = ym;
            }
            ESP_LOGI(TAG, "chip max=(%d,%d)", s_xmax, s_ymax);
            s_ready = 1;
            return 0;
        }
    }
    ESP_LOGE(TAG, "GT911 not found");
    return -1;
}

/* 轮询触摸: 返回 1=读取成功, *pressed=当前是否按下, 按下时给出坐标.
 * 每次调用都返回当前状态 (fxtk.c 内部做按下/抬起/拖动边沿检测) */
int fx_gt911_read(int *x, int *y, int *pressed)
{
    if (!s_ready)
        return 0;
    uint8_t st;
    if (gt911_r(0x814E, &st, 1) != 0)
        return 0;
    if (st & 0x80)
        gt911_w8(0x814E, 0x00);          /* 清 buffer ready */
    int npoints = st & 0x0F;
    int down = (npoints > 0 && npoints < 6);
    *pressed = down;
    if (down) {
        uint8_t buf[4];
        if (gt911_r(0x8150, buf, 4) != 0)
            return 0;
        int px = buf[0] | (buf[1] << 8); /* 小端 */
        int py = buf[2] | (buf[3] << 8);
        if ((px == 0 && py == 0) || px >= 4096 || py >= 4096)
            return 0;
        *x = px;
        *y = py;
    }
    return 1;
}
#include "fxtk.h"
#include "esp_log.h"
#include <SDL2/SDL.h>
#include <X11/Xlib.h>

extern fx_driver_t fx_sdl_driver;
extern void sdl_update_screen(void);
extern void sdl_handle_events(void);
extern void fxtk_font_init(const char *font_path, int size);
extern int sdl_get_width(void);   // 【新增】声明
extern int sdl_get_height(void);  // 【新增】声明
extern void app_init(void);

int main(int argc, char *argv[]) {
    XInitThreads();  /* Xlib 多线程安全, EGL 子线程保险 */
    ESP_LOGI("SYS", "Starting fxtk PC Simulator Engine (Resizable)...");
    
    const char *font_paths[] = {
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        NULL
    };
    
    int font_loaded = 0;
    for (int i = 0; font_paths[i] != NULL; i++) {
        fxtk_font_init(font_paths[i], 18);
        font_loaded = 1;
        ESP_LOGI("SYS", "Attempted font: %s", font_paths[i]);
        break;
    }
    
    if (!font_loaded) {
        ESP_LOGE("SYS", "No font loaded! Text will be broken.");
    }
    
    fx_sdl_driver.init();
    
    // 【新增】更新驱动尺寸为实际窗口大小
    fx_sdl_driver.width = sdl_get_width();
    fx_sdl_driver.height = sdl_get_height();
    
    fx_init(&fx_sdl_driver);
    fx_set_bg(FX_RGB(245, 245, 245));
    
    app_init();
    
    ESP_LOGI("SYS", "UI Ready. Entering main loop (window is resizable)...");
    
    while (1) {
        sdl_handle_events();  // 会处理 resize 事件
        fx_poll();
        sdl_update_screen();
        vTaskDelay(16);
    }
    
    return 0;
}
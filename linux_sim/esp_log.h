#ifndef ESP_LOG_H
#define ESP_LOG_H

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

// 模拟 ESP_LOG
#define ESP_LOGI(tag, fmt, ...) printf("\033[32m[I] %s:\033[0m " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("\033[31m[E] %s:\033[0m " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("\033[33m[W] %s:\033[0m " fmt "\n", tag, ##__VA_ARGS__)

// 模拟 FreeRTOS
#define pdMS_TO_TICKS(ms) (ms)
static inline void vTaskDelay(uint32_t ms) { 
    usleep(ms * 1000); 
}

#endif

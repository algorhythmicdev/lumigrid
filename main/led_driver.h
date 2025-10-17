#pragma once
#include <stdint.h>
#include <esp_err.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w; // Only used for RGBW LEDs
} pixel_t;

esp_err_t led_driver_init(void);
void led_render_task(void *pvParameter);
esp_err_t led_set_pixel(uint16_t index, pixel_t pixel);
esp_err_t led_set_all(pixel_t pixel);
esp_err_t led_show(void);
esp_err_t led_clear(void);

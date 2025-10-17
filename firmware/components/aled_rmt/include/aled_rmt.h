#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "effects.h"

typedef struct {
    int channel_id;
    gpio_num_t gpio;
    uint16_t num_leds;
    led_type_t type;
    void *rmt_channel;
} aled_rmt_handle_t;

esp_err_t aled_rmt_init_channel(aled_rmt_handle_t *handle, gpio_num_t pin, uint16_t num_leds, led_type_t type);
esp_err_t aled_rmt_write_pixels(aled_rmt_handle_t *handle, const px_rgba_t *pixels, uint16_t count);
esp_err_t aled_rmt_clear(aled_rmt_handle_t *handle);
esp_err_t aled_rmt_deinit(aled_rmt_handle_t *handle);

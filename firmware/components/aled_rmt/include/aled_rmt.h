#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "effects.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t aled_rmt_init_chan(int idx, gpio_num_t pin);
esp_err_t aled_rmt_write(int idx, const px_rgba_t* fb, int npx, led_type_t type);
void      aled_rmt_deinit(int idx);

#ifdef __cplusplus
}
#endif

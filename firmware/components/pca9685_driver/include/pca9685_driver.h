#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_err.h"

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    uint8_t oe_pin;
} pca9685_config_t;

esp_err_t pca9685_init(const pca9685_config_t *cfg);
esp_err_t pca9685_set_pwm_freq(uint16_t freq_hz);
esp_err_t pca9685_set_duty(uint8_t channel, float duty);
esp_err_t pca9685_fade_to(uint8_t channel, float target_duty, uint32_t duration_ms, bool log_curve);
esp_err_t pca9685_all_off(void);
esp_err_t pca9685_deinit(void);

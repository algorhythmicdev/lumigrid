#pragma once

#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCA9685_CHANNEL_COUNT 16

typedef struct {
  i2c_port_t i2c_port;
  uint8_t i2c_addr;            // 7-bit address (0x40 default)
  gpio_num_t sda_gpio;
  gpio_num_t scl_gpio;
  gpio_num_t oe_gpio;          // output enable pin (active low); set to -1 if unused
  uint32_t i2c_clk_speed_hz;   // defaults to 400000 when zero
  float pwm_freq_hz;           // defaults to 1000.0f when zero
} pca9685_config_t;

/**
 * @brief Initialize the PCA9685 driver and hardware.
 */
esp_err_t pca9685_init(const pca9685_config_t *cfg);

/**
 * @brief Immediately set duty cycle for a given channel.
 * @param channel LED output channel (0-15).
 * @param duty01 Duty cycle in range [0.0f, 1.0f].
 */
esp_err_t pca9685_set_duty(uint8_t channel, float duty01);

/**
 * @brief Fade a channel to duty over duration in milliseconds.
 * The fade is managed by the driver tick and uses either linear or logarithmic interpolation.
 */
esp_err_t pca9685_fade_to(uint8_t channel, float duty01, uint32_t duration_ms, bool log_curve);

/**
 * @brief Force all outputs off (OE low, then restore high).
 */
esp_err_t pca9685_all_off(void);

/**
 * @brief Advance fade timelines; should be called periodically (e.g., from PWM driver task).
 */
void pca9685_tick(void);

/**
 * @brief Reset fade state and cached duties without touching hardware (useful for tests).
 */
void pca9685_reset_state(void);

typedef esp_err_t (*pca9685_i2c_stub_t)(uint8_t reg, const uint8_t *data, size_t len);

/**
 * @brief Override low level I2C register writes for unit testing.
 */
void pca9685_install_i2c_stub(pca9685_i2c_stub_t stub);
void pca9685_remove_i2c_stub(void);

float pca9685_get_cached_duty(uint8_t channel);

#ifdef __cplusplus
}
#endif

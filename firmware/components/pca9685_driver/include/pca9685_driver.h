#ifndef PCA9685_DRIVER_H
#define PCA9685_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_err.h"

/**
 * @brief Initialize the PCA9685 driver.
 *
 * This function sets up the I2C communication and configures the PCA9685
 * with an appropriate prescaler for a ~1 kHz PWM frequency.
 *
 * @return esp_err_t ESP_OK on success, or an error code from the I2C driver.
 */
/**
 * @brief Configuration structure for the PCA9685 driver.
 */
typedef struct {
    i2c_port_t i2c_port;        ///< I2C port number
    uint8_t i2c_addr;           ///< I2C address of the PCA9685
    gpio_num_t sda_gpio;        ///< GPIO pin for I2C SDA
    gpio_num_t scl_gpio;        ///< GPIO pin for I2C SCL
    gpio_num_t oe_gpio;         ///< GPIO pin for Output Enable (OE)
    uint32_t i2c_clk_speed_hz;  ///< I2C clock speed in Hz
    float pwm_freq_hz;          ///< Target PWM frequency in Hz
} pca9685_config_t;

esp_err_t pca9685_init(const pca9685_config_t *config);

/**
 * @brief Set the PWM duty cycle for a specific channel.
 *
 * @param channel The channel to update (0-15).
 * @param duty The duty cycle, from 0.0 (fully off) to 1.0 (fully on).
 */
void pca9685_set_duty(uint8_t channel, float duty);

/**
 * @brief Fade a channel to a target duty cycle over a specified duration.
 *
 * @param channel The channel to update (0-15).
 * @param target_duty The target duty cycle (0.0 to 1.0).
 * @param duration_ms The fade duration in milliseconds.
 * @param logarithmic Whether to use a logarithmic or linear fade curve.
 */
void pca9685_fade_to(uint8_t channel, float target_duty, uint32_t duration_ms, bool logarithmic);

/**
 * @brief Instantly turn off all PWM channels.
 *
 * This function uses the Output Enable (OE) pin for immediate hardware shutdown.
 * The previous state is restored after a brief delay.
 */
void pca9685_all_off(void);

#endif // PCA9685_DRIVER_H
